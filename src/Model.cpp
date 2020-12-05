#include "Model.h"
#include "util.h"
#include <iostream>
#include <fstream>

Model::Model(string fpath)
{
	this->fpath = fpath;
	int len;
	char * buffer = loadFile(fpath, len);
	data = mstream(buffer, len);
	header = (studiohdr_t*)buffer;
}

Model::~Model()
{
	delete[] data.getBuffer();
}

bool Model::validate() {
	if (string(header->name).length() <= 0) {
		return false;
	}

	if (header->id != 1414743113) {
		cout << "ERROR: Invalid ID in model header\n";
		return false;
	}

	if (header->version != 10) {
		cout << "ERROR: Invalid version in model header\n";
		return false;
	}

	if (header->numseqgroups >= 10000) {
		cout << "ERROR: Too many seqgroups (" + to_string(header->numseqgroups) + ") in model\n";
		return false;
	}

	// Try loading required model info
	data.seek(header->bodypartindex);
	mstudiobodyparts_t* bod = (mstudiobodyparts_t*)data.get();

	if (data.eom())
		return false;

	for (int i = 0; i < bod->nummodels; i++) {
		data.seek(bod->modelindex + i * sizeof(mstudiomodel_t));
		mstudiomodel_t* mod = (mstudiomodel_t*)data.get();

		if (data.eom()) {
			cout << "ERROR: Failed to load body " + to_string(i) + "/" + to_string(bod->nummodels) + "\n";
			return false;
		}
	}

	return true;
}

bool Model::isEmpty() {
	bool isEmptyModel = true;

	data.seek(header->bodypartindex);
	mstudiobodyparts_t* bod = (mstudiobodyparts_t*)data.get();
	for (int i = 0; i < bod->nummodels; i++) {
		data.seek(bod->modelindex + i * sizeof(mstudiomodel_t));
		mstudiomodel_t* mod = (mstudiomodel_t*)data.get();

		if (mod->nummesh != 0) {
			isEmptyModel = false;
			break;
		}
	}

	return isEmptyModel;
}

bool Model::hasExternalTextures() {
	// textures aren't needed if the model has no triangles
	return header->numtextures == 0 && !isEmpty();
}

bool Model::hasExternalSequences() {
	return header->numseqgroups > 1;
}

void Model::insertData(void* src, size_t bytes) {
	data.insert(src, bytes);
	header = (studiohdr_t*)data.getBuffer();
}

void Model::removeData(size_t bytes) {
	data.remove(bytes);
	header = (studiohdr_t*)data.getBuffer();
}

bool Model::mergeExternalTextures(bool deleteSource) {
	if (!hasExternalTextures()) {
		cout << "No external textures to merge\n";
		return false;
	}

	int lastDot = fpath.find_last_of(".");
	string ext = fpath.substr(lastDot);
	string basepath = fpath.substr(0, lastDot);
	string tpath = basepath + "t" + ext;

	if (!fileExists(tpath)) {
		tpath = basepath + "T" + ext;
		if (!fileExists(tpath)) {
			cout << "External texture model not found: " << tpath << endl;
			return false;
		}
	}

	int lastSlash = tpath.find_last_of("/\\");
	string fname = tpath.substr(lastSlash + 1);
	cout << "Merging " << fname << "\n";

	Model tmodel(tpath);

	// copy info
	header->numtextures = tmodel.header->numtextures;
	header->numskinref = tmodel.header->numskinref;
	header->numskinfamilies = tmodel.header->numskinfamilies;

	// recalculate indexes
	size_t actualtextureindex = data.size();
	size_t tmodel_textureindex = tmodel.header->textureindex;
	size_t tmodel_skinindex = tmodel.header->skinindex;
	size_t tmodel_texturedataindex = tmodel.header->texturedataindex;
	header->textureindex = actualtextureindex;
	header->skinindex = actualtextureindex + (tmodel_skinindex - tmodel_textureindex);
	header->texturedataindex = actualtextureindex + (tmodel_texturedataindex - tmodel_textureindex);

	// texture data is at the end of the file, with the structures grouped together
	data.seek(0, SEEK_END);
	tmodel.data.seek(tmodel_textureindex);
	insertData(tmodel.data.get(), tmodel.data.size() - tmodel.data.tell());

	// TODO: Align pointers or else model will crash or something? My test model has everything aligned.
	uint aligned = ((uint)header->texturedataindex + 3) & ~3;
	if (header->texturedataindex != aligned) {
		cout << "dataindex not aligned " << header->texturedataindex << endl;
	}
	aligned = ((uint)header->textureindex + 3) & ~3;
	if (header->textureindex != aligned) {
		cout << "texindex not aligned " << header->textureindex << endl;
	}
	aligned = ((uint)header->skinindex + 3) & ~3;
	if (header->skinindex != aligned) {
		cout << "skinindex not aligned " << header->skinindex << endl;
	}

	// recalculate indexes in the texture infos
	for (int i = 0; i < header->numtextures; i++) {
		data.seek(header->textureindex + i * sizeof(mstudiotexture_t));
		mstudiotexture_t* texture = (mstudiotexture_t*)data.get();
		texture->index = actualtextureindex + (texture->index - tmodel_textureindex);
	}

	header->length = data.size();

	if (deleteSource)
		remove(tpath.c_str());

	return true;
}

#define MOVE_INDEX(val, afterIdx, delta) { \
	if (val >= afterIdx) { \
		val += delta; \
		/*cout << "Updated: " << #val << endl;*/ \
	} \
}

void Model::updateIndexes(int afterIdx, int delta) {
	// skeleton
	MOVE_INDEX(header->boneindex, afterIdx, delta);
	MOVE_INDEX(header->bonecontrollerindex, afterIdx, delta);
	MOVE_INDEX(header->attachmentindex, afterIdx, delta);
	MOVE_INDEX(header->hitboxindex, afterIdx, delta);

	// sequences
	MOVE_INDEX(header->seqindex, afterIdx, delta);
	MOVE_INDEX(header->seqgroupindex, afterIdx, delta);
	MOVE_INDEX(header->transitionindex, afterIdx, delta);
	for (int k = 0; k < header->numseq; k++) {
		data.seek(header->seqindex + k * sizeof(mstudioseqdesc_t));
		mstudioseqdesc_t* seq = (mstudioseqdesc_t*)data.get();

		MOVE_INDEX(seq->eventindex, afterIdx, delta);
		MOVE_INDEX(seq->animindex, afterIdx, delta);
		MOVE_INDEX(seq->pivotindex, afterIdx, delta);
		MOVE_INDEX(seq->automoveposindex, afterIdx, delta);		// unused?
		MOVE_INDEX(seq->automoveangleindex, afterIdx, delta);	// unused?
	}

	// meshes
	MOVE_INDEX(header->bodypartindex, afterIdx, delta);
	for (int i = 0; i < header->numbodyparts; i++) {
		data.seek(header->bodypartindex + i*sizeof(mstudiobodyparts_t));
		mstudiobodyparts_t* bod = (mstudiobodyparts_t*)data.get();
		MOVE_INDEX(bod->modelindex, afterIdx, delta);
		for (int k = 0; k < bod->nummodels; k++) {
			data.seek(bod->modelindex + k * sizeof(mstudiomodel_t));
			mstudiomodel_t* mod = (mstudiomodel_t*)data.get();

			MOVE_INDEX(mod->meshindex, afterIdx, delta);
			for (int j = 0; j < mod->nummesh; j++) {
				data.seek(mod->meshindex + j * sizeof(mstudiomesh_t));
				mstudiomesh_t* mesh = (mstudiomesh_t*)data.get();
				MOVE_INDEX(mesh->normindex, afterIdx, delta, "normindex"); // TODO: is this a file index?
				MOVE_INDEX(mesh->triindex, afterIdx, delta);
			}
			MOVE_INDEX(mod->normindex, afterIdx, delta);
			MOVE_INDEX(mod->norminfoindex, afterIdx, delta);
			MOVE_INDEX(mod->vertindex, afterIdx, delta);
			MOVE_INDEX(mod->vertinfoindex, afterIdx, delta);
		}
	}

	// textures
	MOVE_INDEX(header->textureindex, afterIdx, delta);
	for (int i = 0; i < header->numtextures; i++) {
		data.seek(header->textureindex + i * sizeof(mstudiotexture_t));
		mstudiotexture_t* texture = (mstudiotexture_t*)data.get();
		MOVE_INDEX(texture->index, afterIdx, delta);
	}
	MOVE_INDEX(header->skinindex, afterIdx, delta);
	MOVE_INDEX(header->texturedataindex, afterIdx, delta);

	// sounds (unused?)
	MOVE_INDEX(header->soundindex, afterIdx, delta);
	MOVE_INDEX(header->soundgroupindex, afterIdx, delta);

	header->length = data.size();
}

bool Model::mergeExternalSequences(bool deleteSource) {
	if (!hasExternalSequences()) {
		cout << "No external sequences to merge\n";
		return false;
	}

	int lastDot = fpath.find_last_of(".");
	string ext = fpath.substr(lastDot);
	string basepath = fpath.substr(0, lastDot);

	// save external animations to the end of the file.
	data.seek(0, SEEK_END);

	// save old values before they're overwritten in index updates
	int* oldanimindexes = new int[header->numseq];
	for (int k = 0; k < header->numseq; k++) {
		data.seek(header->seqindex + k * sizeof(mstudioseqdesc_t));
		mstudioseqdesc_t* seq = (mstudioseqdesc_t*)data.get();
		oldanimindexes[k] = seq->animindex;
	}

	for (int i = 1; i < header->numseqgroups; i++)
	{
		string suffix = i < 10 ? "0" + to_string(i) : to_string(i);
		string spath = basepath + suffix + ext;

		if (!fileExists(spath)) {
			cout << "External sequence model not found: " << spath << endl;
			return false;
		}

		int lastSlash = spath.find_last_of("/\\");
		string fname = spath.substr(lastSlash + 1);
		cout << "Merging " << fname << "\n";

		Model smodel(spath);

		// Sequence models contain a header followed by animation data.
		// This will append those animations after the primary model's animations.
		data.seek(header->seqindex);
		smodel.data.seek(sizeof(studioseqhdr_t));
		size_t insertOffset = data.tell();
		size_t animCopySize = smodel.data.size() - sizeof(studioseqhdr_t);
		insertData(smodel.data.get(), animCopySize);
		updateIndexes(insertOffset, animCopySize);

		// update indexes for the merged animations
		for (int k = 0; k < header->numseq; k++) {
			data.seek(header->seqindex + k * sizeof(mstudioseqdesc_t));
			mstudioseqdesc_t* seq = (mstudioseqdesc_t*)data.get();

			if (seq->seqgroup != i)
				continue;

			seq->animindex = insertOffset + (oldanimindexes[k] - sizeof(studioseqhdr_t));
			seq->seqgroup = 0;
		}

		if (deleteSource)
			remove(spath.c_str());
	}

	delete[] oldanimindexes;

	// remove infos for the merged sequence groups
	data.seek(header->seqgroupindex + sizeof(mstudioseqgroup_t));
	size_t removeOffset = data.tell();
	int removeBytes = sizeof(mstudioseqgroup_t) * (header->numseqgroups - 1);
	removeData(removeBytes);
	header->numseqgroups = 1;
	updateIndexes(removeOffset, -removeBytes);

	return true;
}

bool Model::cropTexture(string cropName, int newWidth, int newHeight) {
	for (int i = 0; i < header->numtextures; i++) {
		data.seek(header->textureindex + i * sizeof(mstudiotexture_t));
		mstudiotexture_t* texture = (mstudiotexture_t*)data.get();
		string name = texture->name;

		if (string(texture->name) != cropName) {
			continue;
		}

		cout << "Cropping " << cropName << " from " << texture->width << "x" << texture->height <<
			" to " << newWidth << "x" << newHeight << endl;

		data.seek(texture->index);
		int oldSize = texture->width * texture->height;
		int newSize = newWidth * newHeight;
		int palSize = 256 * 3;
		byte* oldTexData = new byte[oldSize];
		byte* palette = new byte[palSize];
		byte* newTexData = new byte[newSize];
		data.read(oldTexData, oldSize);
		data.read(palette, palSize);
		
		for (int y = 0; y < newHeight; y++) {
			for (int x = 0; x < newWidth; x++) {
				int oldY = y >= texture->height ? texture->height-1 : y;
				int oldX = x >= texture->width ? texture->width -1 : x;
				newTexData[y * newWidth + x] = oldTexData[oldY*texture->width + oldX];
			}
		}

		data.seek(texture->index);
		data.write(newTexData, newSize);
		data.write(palette, palSize);

		texture->width = newWidth;
		texture->height = newHeight;

		size_t removeAt = data.tell();
		int removeBytes = oldSize - newSize;
		removeData(removeBytes);
		updateIndexes(removeAt, -removeBytes);

		header->length = data.size();

		return true;
	}

	cout << "ERROR: No texture found with name '" << cropName << "'\n";
	return false;
}

bool Model::renameTexture(string oldName, string newName) {
	for (int i = 0; i < header->numtextures; i++) {
		data.seek(header->textureindex + i * sizeof(mstudiotexture_t));
		mstudiotexture_t* texture = (mstudiotexture_t*)data.get();

		if (string(texture->name) == oldName) {
			strncpy(texture->name, newName.c_str(), 64);
			return true;
		}
	}

	cout << "ERROR: No texture found with name '" << oldName << "'\n";
	return false;
}

void Model::write(string fpath) {
	fstream fout = fstream(fpath.c_str(), std::ios::out | std::ios::binary);
	fout.write(data.getBuffer(), data.size());
	cout << "Wrote " << fpath << " (" << data.size() << " bytes)\n";
}
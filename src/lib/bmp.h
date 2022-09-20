/* \file bmp.h
 *  BMP image format
 *
 *  Author:	rjianwang
 *	Date:	2016-09-06
 *  Email:  rjianwang@foxmail.com
 */

#pragma once

#include <string>

typedef long			LONG;
typedef unsigned long	DWORD;
typedef int				BOOL;
typedef unsigned char   BYTE;
typedef unsigned short	WORD;
typedef BYTE* LPBYTE;
typedef DWORD* LPDWORD;


#pragma pack(1)
typedef struct tagRGBQUAD
{
	BYTE	rgbBlue;
	BYTE	rgbGreen;
	BYTE	rgbRed;
	BYTE	rgbReserved;
}RGBQUAD;

typedef struct  tagBITMAPFILEHEADER
{
	WORD	bfType;				// �ļ����ͣ�������0x424D�����ַ���BM��   
	DWORD	bfSize;				// �ļ���С   
	WORD	bfReserved1;		// ������   
	WORD	bfReserved2;		// ������   
	DWORD	bfOffBits;			// ���ļ�ͷ��ʵ��λͼ���ݵ�ƫ���ֽ���   
}BITMAPFILEHEADER;				// λͼ�ļ�ͷ���� 

typedef struct tagBITMAPINFOHEADER
{
	DWORD	biSize;				// ��Ϣͷ��С   
	LONG	biWidth;			// ͼ����   
	LONG	biHeight;			// ͼ��߶�   
	WORD	biPlanes;			// λƽ����������Ϊ1   
	WORD	biBitCount;			// ÿ����λ��: 1, 2, 4, 8, 16, 24, 32
	DWORD	biCompression;		// ѹ������   
	DWORD	biSizeImage;		// ѹ��ͼ���С�ֽ���   
	LONG	biXPelsPerMeter;	// ˮƽ�ֱ���   
	LONG	biYPelsPerMeter;	// ��ֱ�ֱ���   
	DWORD	biClrUsed;			// λͼʵ���õ���ɫ����   
	DWORD	biClrImportant;		// ��λͼ����Ҫ��ɫ����   
}BITMAPINFOHEADER;				// λͼ��Ϣͷ����   
#pragma pack()

// class BMP
//
// BMP is an image file format that stores bitmap digital images and retains 
// information for each pixel of the image. The BMP format stores color data 
// for each pixel in the image without any compression. For example, a 10x10 
// pixel BMP image will include color data for 100 pixels. This method of 
// storing image information allows for crisp, high-quality graphics, but 
// also produces large file sizes. 
class BMP
{
public:
	BMP();
	BMP(const std::string& location);

	~BMP();

public:
	BYTE* load(const std::string& location);
	void	save(const std::string& location);
private:
	void	swap(BYTE** src, DWORD width, DWORD height, WORD channel);

public:
	LONG	width();
	LONG	height();
	BYTE	depth();
	BYTE* data();
	WORD	_channels();

public:
	DWORD	rows;
	DWORD	cols;
	WORD	channels;
	BITMAPFILEHEADER* head;
	BITMAPINFOHEADER* info;
	RGBQUAD* palette;   // color table
	BYTE* pixels;	// iamge pixel data  

}; /* end for class BMP */
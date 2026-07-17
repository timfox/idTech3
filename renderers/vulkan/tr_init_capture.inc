/* Renderer screenshot, levelshot, and video-frame capture helpers.
 * Included by tr_init.c to preserve renderer backend ABI. */

/*
==================
RB_ReadPixels

Reads an image but takes care of alignment issues for reading RGB images.

Reads a minimum offset for where the RGB data starts in the image from
integer stored at pointer offset. When the function has returned the actual
offset was written back to address offset. This address will always have an
alignment of packAlign to ensure efficient copying.

Stores the length of padding after a line of pixels to address padlen

Return value must be freed with ri.Hunk_FreeTempMemory()
==================
*/
static byte *RB_ReadPixels(int x, int y, int width, int height, size_t *offset, int *padlen, int lineAlign )
{
#ifdef USE_VULKAN
	byte *buffer, *bufstart;
	int linelen;
	int	bufAlign;
	int packAlign = 1;

	(void)x;
	(void)y;
	(void)lineAlign;

	linelen = width * 3;

	bufAlign = MAX( packAlign, 16 ); // for SIMD

	// Allocate a few more bytes so that we can choose an alignment we like
	//buffer = ri.Hunk_AllocateTempMemory(padwidth * height + *offset + bufAlign - 1);
	buffer = ri.Hunk_AllocateTempMemory(width * height * 4 + *offset + bufAlign - 1);
	bufstart = PADP((intptr_t) buffer + *offset, bufAlign);

	vk_read_pixels( bufstart, width, height );

	*offset = bufstart - buffer;
	*padlen = PAD(linelen, packAlign) - linelen;

	return buffer;
#else
	byte *buffer, *bufstart;
	int padwidth, linelen;
	int	bufAlign;
	GLint packAlign;

	qglGetIntegerv(GL_PACK_ALIGNMENT, &packAlign);

	linelen = width * 3;

	if ( packAlign < lineAlign )
		padwidth = PAD(linelen, lineAlign);
	else
		padwidth = PAD(linelen, packAlign);

	bufAlign = MAX( packAlign, 16 ); // for SIMD

	// Allocate a few more bytes so that we can choose an alignment we like
	buffer = ri.Hunk_AllocateTempMemory(padwidth * height + *offset + bufAlign - 1);
	bufstart = PADP((intptr_t) buffer + *offset, bufAlign);

	qglReadPixels( x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, bufstart );

	*offset = bufstart - buffer;
	*padlen = PAD(linelen, packAlign) - linelen;

	return buffer;
#endif
}


/*
==================
RB_TakeScreenshot
==================
*/
void RB_TakeScreenshot( int x, int y, int width, int height, const char *fileName )
{
	const int header_size = 18;
	byte *allbuf, *buffer;
	byte *srcptr, *destptr;
	byte *endline, *endmem;
	byte temp;
	int linelen, padlen;
	size_t offset, memcount;

	offset = header_size;
	allbuf = RB_ReadPixels( x, y, width, height, &offset, &padlen, 0 );
	buffer = allbuf + offset - header_size;

	Com_Memset( buffer, 0, header_size );
	buffer[2] = 2;		// uncompressed type
	buffer[12] = width & 255;
	buffer[13] = width >> 8;
	buffer[14] = height & 255;
	buffer[15] = height >> 8;
	buffer[16] = 24;	// pixel size

	// swap rgb to bgr and remove padding from line endings
	linelen = width * 3;

	srcptr = destptr = allbuf + offset;
	endmem = srcptr + (linelen + padlen) * height;

	while(srcptr < endmem)
	{
		endline = srcptr + linelen;

		while(srcptr < endline)
		{
			temp = srcptr[0];
			*destptr++ = srcptr[2];
			*destptr++ = srcptr[1];
			*destptr++ = temp;

			srcptr += 3;
		}

		// Skip the pad
		srcptr += padlen;
	}

	memcount = linelen * height;

	// gamma correction
	R_GammaCorrect( allbuf + offset, memcount );

	ri.FS_WriteFile( fileName, buffer, memcount + header_size );

	ri.Hunk_FreeTempMemory( allbuf );
}


/*
==================
RB_TakeScreenshotJPEG
==================
*/
void RB_TakeScreenshotJPEG( int x, int y, int width, int height, const char *fileName )
{
	byte *buffer;
	size_t offset = 0, memcount;
	int padlen;

	buffer = RB_ReadPixels(x, y, width, height, &offset, &padlen, 0);
	memcount = (width * 3 + padlen) * height;

	// gamma correction
	R_GammaCorrect( buffer + offset, memcount );

	ri.CL_SaveJPG( fileName, r_screenshotJpegQuality->integer, width, height, buffer + offset, padlen );
	ri.Hunk_FreeTempMemory( buffer );
}


static void FillBMPHeader( byte *buffer, int width, int height, int memcount, int header_size )
{
	int filesize;
	Com_Memset( buffer, 0, header_size );

	// bitmap file header
	buffer[0] = 'B';
	buffer[1] = 'M';
	filesize = memcount + header_size;
	buffer[2] = (filesize >> 0) & 255;
	buffer[3] = (filesize >> 8) & 255;
	buffer[4] = (filesize >> 16) & 255;
	buffer[5] = (filesize >> 24) & 255;
	buffer[10] = header_size; // data offset

	// bitmap info header
	buffer[14] = 40; // size of this header
	buffer[18] = (width >> 0) & 255;
	buffer[19] = (width >> 8) & 255;
	buffer[20] = (width >> 16) & 255;
	buffer[21] = (width >> 24) & 255;

	buffer[22] = (height >> 0) & 255;
	buffer[23] = (height >> 8) & 255;
	buffer[24] = (height >> 16) & 255;
	buffer[25] = (height >> 24) & 255;
	buffer[26] = 1; // number of color planes
	buffer[28] = 24; // bpp

	buffer[34] = (memcount >> 0) & 255;
	buffer[35] = (memcount >> 8) & 255;
	buffer[36] = (memcount >> 16) & 255;
	buffer[37] = (memcount >> 24) & 255;
	buffer[38] = 0xC4; // horizontal dpi
	buffer[39] = 0x0E; // horizontal dpi
	buffer[42] = 0xC4; // vertical dpi
	buffer[43] = 0x0E; // vertical dpi
}


/*
==================
RB_TakeScreenshotBMP
==================
*/
void RB_TakeScreenshotBMP( int x, int y, int width, int height, const char *fileName, int clipboardOnly )
{
	byte *allbuf;
	byte *buffer; // destination buffer
	byte *srcptr, *srcline;
	byte *destptr, *dstline;
	byte *endmem;
	byte temp[4];
	size_t memcount, offset;
	const int header_size = 54; // bitmapfileheader(14) + bitmapinfoheader(40)
	int scanlen, padlen;
	int scanpad, len;

	offset = header_size;

	allbuf = RB_ReadPixels( x, y, width, height, &offset, &padlen, 4 );
	buffer = allbuf + offset;

	// scanline length
	scanlen = PAD( width*3, 4 );
	scanpad = scanlen - width*3;
	memcount = scanlen * height;

	// swap rgb to bgr and add line padding
	if ( scanpad == 0 && padlen == 0 ) {
		// fastest case
		srcptr = destptr = allbuf + offset;
		endmem = srcptr + scanlen * height;
		while ( srcptr < endmem ) {
			temp[0] = srcptr[0];
			destptr[0] = srcptr[2];
			destptr[2] = temp[0];
			destptr += 3;
			srcptr += 3;
		}
	} else {
		// move destination buffer forward if source padding is greater than for BMP
		if ( padlen > scanpad )
			buffer += (width * 3 + padlen - scanlen ) * height;
		// point on last line
		srcptr = allbuf + offset + (height-1) * (width * 3 + padlen);
		destptr = buffer + (height-1) * scanlen;
		len = (width * 3 - 3);
		while ( destptr >= buffer ) {
			srcline = srcptr + len;
			dstline = destptr + len;
			while ( srcline >= srcptr ) {
				temp[2] = srcline[0];
				temp[1] = srcline[1];
				temp[0] = srcline[2];
				dstline[0] = temp[0];
				dstline[1] = temp[1];
				dstline[2] = temp[2];
				dstline-=3;
				srcline-=3;
			}
			srcptr -= (width * 3 + padlen);
			destptr -= scanlen;
		}
	}

	// fill this last to avoid data overwrite in case when we're moving destination buffer forward
	FillBMPHeader( buffer - header_size, width, height, memcount, header_size );

	// gamma correction
	R_GammaCorrect( buffer, memcount );

	if ( clipboardOnly ) {
		// copy starting from bitmapinfoheader
		ri.Sys_SetClipboardBitmap( buffer - 40, memcount + 40 );
	} else {
		ri.FS_WriteFile( fileName, buffer - header_size, memcount + header_size );
	}

	ri.Hunk_FreeTempMemory( allbuf );
}


/*
==================
R_ScreenshotFilename
==================
*/
static void R_ScreenshotFilename( char *fileName, const char *fileExt ) {
	qtime_t t;
	int count;

	count = 0;
	ri.Com_RealTime( &t );

	Com_sprintf( fileName, MAX_OSPATH, "screenshots/shot-%04d%02d%02d-%02d%02d%02d.%s",
			1900 + t.tm_year, 1 + t.tm_mon,	t.tm_mday,
			t.tm_hour, t.tm_min, t.tm_sec, fileExt );

	while (	ri.FS_FileExists( fileName ) && ++count < 1000 ) {
		Com_sprintf( fileName, MAX_OSPATH, "screenshots/shot-%04d%02d%02d-%02d%02d%02d-%d.%s",
				1900 + t.tm_year, 1 + t.tm_mon,	t.tm_mday,
				t.tm_hour, t.tm_min, t.tm_sec, count, fileExt );
	}
}


/*
====================
R_LevelShot

levelshots are specialized 128*128 thumbnails for
the menu system, sampled down from full screen distorted images
====================
*/
static void R_LevelShot( void ) {
	char		checkname[MAX_OSPATH];
	byte		*buffer;
	byte		*source, *allsource;
	byte		*src, *dst;
	size_t		offset = 0;
	int			padlen;
	int			x, y;
	int			r, g, b;
	float		xScale, yScale;
	int			xx, yy;

	if ( !tr.world || !tr.world->baseName[0] ) {
		ri.Printf( PRINT_WARNING, "Levelshot requires a loaded map.\n" );
		return;
	}
	Com_sprintf(checkname, sizeof(checkname), "levelshots/%s.tga", tr.world->baseName);

	allsource = RB_ReadPixels(0, 0, gls.captureWidth, gls.captureHeight, &offset, &padlen, 0 );
	source = allsource + offset;

	buffer = ri.Hunk_AllocateTempMemory(128 * 128*3 + 18);
	Com_Memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = 128;
	buffer[14] = 128;
	buffer[16] = 24;	// pixel size

	// resample from source
	xScale = glConfig.vidWidth / 512.0f;
	yScale = glConfig.vidHeight / 384.0f;
	for ( y = 0 ; y < 128 ; y++ ) {
		for ( x = 0 ; x < 128 ; x++ ) {
			r = g = b = 0;
			for ( yy = 0 ; yy < 3 ; yy++ ) {
				for ( xx = 0 ; xx < 4 ; xx++ ) {
					src = source + (3 * glConfig.vidWidth + padlen) * (int)((y*3 + yy) * yScale) +
						3 * (int) ((x*4 + xx) * xScale);
					r += src[0];
					g += src[1];
					b += src[2];
				}
			}
			dst = buffer + 18 + 3 * ( y * 128 + x );
			dst[0] = b / 12;
			dst[1] = g / 12;
			dst[2] = r / 12;
		}
	}

	// gamma correction
	R_GammaCorrect( buffer + 18, 128 * 128 * 3 );

	ri.FS_WriteFile( checkname, buffer, 128 * 128*3 + 18 );

	ri.Hunk_FreeTempMemory(buffer);
	ri.Hunk_FreeTempMemory(allsource);

	ri.Printf( PRINT_ALL, "Wrote %s\n", checkname );
}


/*
==================
R_ScreenShot_f

screenshot
screenshot [silent]
screenshot [levelshot]
screenshot [filename]

Doesn't print the pacifier message if there is a second arg
==================
*/
static void R_ScreenShot_f( void ) {
	char		checkname[MAX_OSPATH];
	qboolean	silent;
	int			typeMask;
	const char	*ext;

	if ( ri.CL_IsMinimized() && !RE_CanMinimize() ) {
		ri.Printf( PRINT_WARNING, "WARNING: unable to take screenshot when minimized because FBO is not available/enabled.\n" );
		return;
	}

	if ( !strcmp( ri.Cmd_Argv(1), "levelshot" ) ) {
		R_LevelShot();
		return;
	}

	if ( Q_stricmp( ri.Cmd_Argv(0), "screenshotJPEG" ) == 0 ) {
		typeMask = SCREENSHOT_JPG;
		ext = "jpg";
	} else if ( Q_stricmp( ri.Cmd_Argv(0), "screenshotBMP" ) == 0 ) {
		typeMask = SCREENSHOT_BMP;
		ext = "bmp";
	} else {
		typeMask = SCREENSHOT_TGA;
		ext = "tga";
	}

	// check if already scheduled
	if ( backEnd.screenshotMask & typeMask )
		return;

	if ( !strcmp( ri.Cmd_Argv(1), "silent" ) ) {
		silent = qtrue;
	} else if ( typeMask == SCREENSHOT_BMP && !strcmp( ri.Cmd_Argv(1), "clipboard" ) ) {
		backEnd.screenshotMask |= SCREENSHOT_BMP_CLIPBOARD;
		silent = qtrue;
	} else {
		silent = qfalse;
	}

	if ( ri.Cmd_Argc() == 2 && !silent ) {
		// explicit filename
		Com_sprintf( checkname, MAX_OSPATH, "screenshots/%s.%s", ri.Cmd_Argv( 1 ), ext );
	} else {
		if ( backEnd.screenshotMask & SCREENSHOT_BMP_CLIPBOARD ) {
			// no need for filename, copy to system buffer
			checkname[0] = '\0';
		} else {
			// scan for a free filename
			R_ScreenshotFilename( checkname, ext );
		}
	}

	// we will make screenshot right at the end of RE_EndFrame()
	backEnd.screenshotMask |= typeMask;
	if ( typeMask == SCREENSHOT_JPG ) {
		backEnd.screenShotJPGsilent = silent;
		Q_strncpyz( backEnd.screenshotJPG, checkname, sizeof( backEnd.screenshotJPG ) );
	} else if ( typeMask == SCREENSHOT_BMP ) {
		backEnd.screenShotBMPsilent = silent;
		Q_strncpyz( backEnd.screenshotBMP, checkname, sizeof( backEnd.screenshotBMP ) );
	} else {
		backEnd.screenShotTGAsilent = silent;
		Q_strncpyz( backEnd.screenshotTGA, checkname, sizeof( backEnd.screenshotTGA ) );
	}
}


//============================================================================

/*
==================
RB_TakeVideoFrameCmd
==================
*/
const void *RB_TakeVideoFrameCmd( const void *data )
{
	const videoFrameCommand_t *cmd;
	byte		*cBuf;
	size_t		memcount, linelen;
	int			padwidth, avipadwidth, padlen, avipadlen;
	int			packAlign;

	cmd = (const videoFrameCommand_t *)data;

#ifdef USE_VULKAN
	packAlign = 1;
#else
	qglGetIntegerv(GL_PACK_ALIGNMENT, &packAlign);
#endif

	linelen = cmd->width * 3;

	// Alignment stuff for glReadPixels
	padwidth = PAD(linelen, packAlign);
	padlen = padwidth - linelen;
	// AVI line padding
	avipadwidth = PAD(linelen, AVI_LINE_PADDING);
	avipadlen = avipadwidth - linelen;

	cBuf = PADP(cmd->captureBuffer, packAlign);

#ifdef USE_VULKAN
	vk_read_pixels(cBuf, cmd->width, cmd->height);
#else
	qglReadPixels(0, 0, cmd->width, cmd->height, GL_RGB, GL_UNSIGNED_BYTE, cBuf);
#endif

	memcount = padwidth * cmd->height;

	// gamma correction
	R_GammaCorrect( cBuf, memcount );

	if ( cmd->motionJpeg )
	{
		memcount = ri.CL_SaveJPGToBuffer( cmd->encodeBuffer, linelen * cmd->height,
			r_aviMotionJpegQuality->integer,
			cmd->width, cmd->height, cBuf, padlen );
		ri.CL_WriteAVIVideoFrame(cmd->encodeBuffer, memcount);
	}
	else
	{
		byte *lineend, *memend;
		byte *srcptr, *destptr;

		srcptr = cBuf;
		destptr = cmd->encodeBuffer;
		memend = srcptr + memcount;

		// swap R and B and remove line paddings
		while(srcptr < memend)
		{
			lineend = srcptr + linelen;
			while(srcptr < lineend)
			{
				*destptr++ = srcptr[2];
				*destptr++ = srcptr[1];
				*destptr++ = srcptr[0];
				srcptr += 3;
			}

			Com_Memset(destptr, '\0', avipadlen);
			destptr += avipadlen;

			srcptr += padlen;
		}

		ri.CL_WriteAVIVideoFrame(cmd->encodeBuffer, avipadwidth * cmd->height);
	}

	return (const void *)(cmd + 1);
}


//============================================================================

#include "image.h"
#include <thorin_runtime.h>
#include <png.h>

#include <vector>

namespace rt {

Image::Image()
: Image(64, 64)
{
}

Image::Image(unsigned w, unsigned h)
: surface(nullptr)
, rawmem(nullptr)
{
    alloc(w, h);
    clear();
    mem2surface(w, h);
}

void Image::alloc(unsigned w, unsigned h)
{
    if(rawmem)
    {
        thorin_free(rawmem);
        rawmem = nullptr;
    }
    memsize = w*h*4;
    if(!memsize)
        return;
    rawmem = thorin_malloc(memsize);
}

void Image::mem2surface(unsigned w, unsigned h)
{
    clearSurface();
    if(!rawmem)
        return;
    surface = SDL_CreateRGBSurfaceFrom(rawmem, w, h, 32, w*4, 0, 0, 0, 0); // seems safe to use just 0 everywhere
}

void Image::clearSurface()
{
    if(surface)
    {
        SDL_FreeSurface(surface);
        surface = nullptr;
    }
}

void Image::clear()
{
    if(rawmem)
        memset(rawmem, 0xff, memsize);
}

Image::~Image()
{
    if(surface)
        SDL_FreeSurface(surface);
    thorin_free(rawmem);
}

bool Image::loadPNG(const char *fn)
{
    thorin_free(rawmem);
    size_t w, h;
    unsigned *rawmemi = loadPNGBuf(fn, w, h);
    bool loaded = !!rawmemi;
    rawmem = rawmemi;
    memsize = w*h*4;
    if(!rawmem)
    {
        w = 64;
        h = 64;
        alloc(w, h);
        clear();
    }
    mem2surface((unsigned)w, (unsigned)h);
    return loaded;
}

unsigned *Image::loadPNGBuf(const char *fn, size_t& w, size_t& h)
{
    unsigned *rawmemi = nullptr, *rp = nullptr;
	png_byte header[8];    // 8 is the maximum size that can be checked
	std::vector<png_byte> byteData;
	std::vector<png_byte*> rowData;
	png_infop info_ptr = 0;
	png_structp png_ptr = 0;
	unsigned char channels = 0;
	bool success = true;


	/* open file and test for it being a png */
	FILE *fp = fopen(fn, "rb");
	if (!fp)
	{
	    success = false;
		std::cerr << "[read_png_file] File " << fn << " could not be opened for reading" << std::endl;
		goto end;
	}

	fread(header, 1, 8, fp);
	if (png_sig_cmp(header, 0, 8))
	{
		std::cerr << "[read_png_file] File " << fn << " is not recognized as a PNG file" << std::endl;
		success = false;
		goto end;
	}


	/* initialize stuff */
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);

	if (!png_ptr)
	{
		std::cerr << "[read_png_file] png_create_read_struct failed" << std::endl;
		success = false;
		goto end;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		std::cerr << "[read_png_file] png_create_info_struct failed" << std::endl;
		success = false;
		goto end;
	}

	if (setjmp(png_jmpbuf(png_ptr)))
	{
		std::cerr << "[read_png_file] Error during init_io" << std::endl;
		success = false;
		goto end;
	}

	png_init_io(png_ptr, fp);
	png_set_sig_bytes(png_ptr, 8);

	png_read_info(png_ptr, info_ptr);

    png_set_palette_to_rgb(png_ptr);

	w = png_get_image_width(png_ptr, info_ptr);
	h = png_get_image_height(png_ptr, info_ptr);
	/*color_type = info_ptr->color_type;
	bit_depth = info_ptr->bit_depth;*/

	png_set_interlace_handling(png_ptr);
	png_read_update_info(png_ptr, info_ptr);

	byteData.resize(png_get_rowbytes(png_ptr, info_ptr) * h);
	rowData.resize(h);
	for(unsigned int i = 0; i < h; i++)
		rowData[i] = i * png_get_rowbytes(png_ptr, info_ptr) + &byteData.front();

	/* read file */
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		std::cerr << "[read_png_file] Error during read_image" << std::endl;
		success = false;
		goto end;
	}

	png_read_image(png_ptr, &rowData.front());

	rp = rawmemi = (unsigned*)thorin_malloc(w*h*4);

	channels = png_get_channels(png_ptr, info_ptr);

	switch(channels)
	{
	case 4:
		// ignores the alpha channel
		for(size_t y = 0; y < h; y++)
		{
			png_byte *byte = rowData[y];
			for(size_t x = 0; x < w; x++)
			{
				//float r = (float)(*byte++) / 255.f;
				//float g = (float)(*byte++) / 255.f;
				//float b = (float)(*byte++) / 255.f;
				//byte++;
				//RGBColor c = RGBColor(r, g, b);
				//if (fixGamma) c = c.gammaIn();
				*rp++ = 0xff000000 | (byte[0]) | (byte[1] << 8) | (byte[2] << 16);
				byte += 4;
			}
		}
		break;

	case 3:
		for(size_t y = 0; y < h; y++)
		{
			png_byte *byte = rowData[y];
			for(size_t x = 0; x < w; x++)
			{
				//float r = (float)(*byte++) / 255.f;
				//float g = (float)(*byte++) / 255.f;
				//float b = (float)(*byte++) / 255.f;
				//RGBColor c = RGBColor(r, g, b);
				//if (fixGamma) c = c.gammaIn();
				//pixels(x, y) = c;
				*rp++ = 0xff000000 | (byte[0]) | (byte[1] << 8) | (byte[2] << 16);
				byte += 3;
			}
		}
		break;

	default:
		std::cerr << "ImagePNG: Unsupported channel count: " << (int)channels << std::endl;
		success = false;
		goto end;
	}

	png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);

end:
	if(fp)
		fclose(fp);
    if(!success)
    {
        thorin_free(rawmemi);
        return nullptr;
    }

    std::cout << "ImagePNG: Success: " << success << std::endl;

	return rawmemi;

}

bool Image::writePNGBuf(const char *filename, const unsigned *rawmemi, size_t w, size_t h)
{
	std::vector<png_byte> byteData (w * h * 4);
	png_byte *ptr = &byteData[0];
	const unsigned *readptr = rawmemi;

	for(uint y = 0; y < h; ++y)
		for(uint x = 0; x < w; ++x)
		{
			*ptr++ = (unsigned char)(*readptr);
			*ptr++ = (unsigned char)(*readptr >> 8);
			*ptr++ = (unsigned char)(*readptr >> 16);
			*ptr++ = 0xFF;
			++readptr;
		}

	std::vector<png_byte*> rowData(h);
	for(size_t i = 0; i < h; i++)
		rowData[i] = i * w * 4 + &byteData.front();

	/* create file */
	FILE *fp = fopen(filename, "wb");
	if (!fp) {
		std::cerr << "[write_png_file] File " << filename << " could not be opened for writing." << std::endl;
		return false;
	}

	bool success = true;

	/* initialize stuff */
	png_structp png_ptr;
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);

	if (!png_ptr) {
		std::cerr << "[write_png_file] png_create_write_struct failed" << std::endl;
		success = false;
		goto end;
	}

	png_infop info_ptr;
	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		std::cerr << "[write_png_file] png_create_info_struct failed" << std::endl;
		success = false;
		goto end;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		std::cerr << "[write_png_file] Error during init_io" << std::endl;
		success = false;
		goto end;
	}


	png_init_io(png_ptr, fp);

	/* write header */
	if (setjmp(png_jmpbuf(png_ptr))) {
		std::cerr << "[write_png_file] Error during writing header" << std::endl;
		success = false;
		goto end;
	}

	png_set_IHDR(png_ptr, info_ptr, w, h,
		8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(png_ptr, info_ptr);

	/* write bytes */
	if (setjmp(png_jmpbuf(png_ptr))) {
		std::cerr << "[write_png_file] Error during writing bytes" << std::endl;
		success = false;
		goto end;
	}

	png_write_image(png_ptr, (png_byte**)&rowData.front());


	/* end write */
	if (setjmp(png_jmpbuf(png_ptr))) {
		std::cerr << "[write_png_file] Error during end of write" << std::endl;
		success = false;
		goto end;
	}

	png_write_end(png_ptr, nullptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);

end:
	if(fp)
		fclose(fp);
	return success;
}


} // end namespace rt

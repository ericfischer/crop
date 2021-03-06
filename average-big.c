#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>
#include <ctype.h>
#include <limits.h>

#define WIDTH 640
#define HEIGHT 640

int accum[WIDTH * HEIGHT * 3] = {0};
int frames = 0;

void output(unsigned char *image, int width, int height, int depth, char *filename,
	    int left, int top, int right, int bottom) {
	int quality = 75;
	static int count = 0;

	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	FILE *outfile;		 /* target file */
	JSAMPROW row_pointer[1]; /* pointer to JSAMPLE row[s] */
	int row_stride;		 /* physical row width in image buffer */

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	if ((outfile = fopen(filename, "wb")) == NULL) {
		fprintf(stderr, "can't open %s\n", filename);
		exit(1);
	}
	jpeg_stdio_dest(&cinfo, outfile);

	cinfo.image_width = right - left; /* image width and height, in pixels */
	cinfo.image_height = bottom - top;
	cinfo.input_components = depth; /* # of color components per pixel */
	cinfo.in_color_space = JCS_RGB; /* colorspace of input image */
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);

	jpeg_start_compress(&cinfo, TRUE);

	row_stride = (right - left) * depth; /* JSAMPLEs per row in image_buffer */

	int y = top;
	while (cinfo.next_scanline < cinfo.image_height) {
		/* jpeg_write_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could pass
     * more than one scanline at a time if that's more convenient.
     */
		row_pointer[0] = &image[y * depth * width + left * depth];
		//row_pointer[0] = & image_buffer[cinfo.next_scanline * row_stride];
		(void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
		y++;
	}

	/* Step 6: Finish compression */

	jpeg_finish_compress(&cinfo);
	/* After finish_compress, we can close the output file. */
	fclose(outfile);

	/* Step 7: release JPEG compression object */

	/* This is an important step since it will release a good deal of memory. */
	jpeg_destroy_compress(&cinfo);

	/* And we're done! */
}

void process(unsigned char *image, int width, int height, int depth, char *filename,
	     int left, int top, int right, int bottom, char *prefix) {
	printf("processing %d by %d, depth %d\n", width, height, depth);

	if (width != WIDTH || height != HEIGHT) {
		printf("argh!\n");
		return;
	}

	unsigned char outbuf[width * height * depth];

	int x, y, d;

	frames++;
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			for (d = 0; d < 3; d++) {
				accum[d + x * 3 + y * 3 * width] += image[d + x * 3 + y * 3 * width];
			}
		}
	}

	int min = INT_MAX;
	int max = 0;

	int i;
	for (i = 0; i < width * height * 3; i++) {
		if (accum[i] < min) {
			min = accum[i];
		}
		if (accum[i] > max) {
			max = accum[i];
		}
	}

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			for (d = 0; d < 3; d++) {
				outbuf[d + x * 3 + y * 3 * width] = (accum[d + x * 3 + y * 3 * width] - min + 0.0f) / (max - min) * 255;
			}
		}
	}

	char fname[200];
	sprintf(fname, "%s%d.jpg", prefix, frames / 1000);
	printf("%d\n", frames);

	output(outbuf, width, height, depth, fname, 0, 0, width, height);

	return;
}

void read_JPEG_file(char *filename, char *prefix) {
	struct jpeg_decompress_struct cinfo;
	FILE *infile;      /* source file */
	JSAMPARRAY buffer; /* Output row buffer */
	int row_stride;    /* physical row width in output buffer */

	unsigned char *image;
	unsigned char *where;

	if ((infile = fopen(filename, "rb")) == NULL) {
		fprintf(stderr, "can't open %s\n", filename);
		return;
	}

	struct jpeg_error_mgr jerr;
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, infile);

	(void) jpeg_read_header(&cinfo, TRUE);
	(void) jpeg_start_decompress(&cinfo);

	/* JSAMPLEs per row in output buffer */
	row_stride = cinfo.output_width * cinfo.output_components;
	/* Make a one-row-high sample array that will go away when done with image */
	buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

	image = malloc(cinfo.output_width * cinfo.output_height * cinfo.output_components);
	if (image == NULL) {
		fprintf(stderr, "failed to allocate memory\n");
		exit(EXIT_FAILURE);
	}
	where = image;

	while (cinfo.output_scanline < cinfo.output_height) {
		(void) jpeg_read_scanlines(&cinfo, buffer, 1);

		memcpy(where, buffer[0], row_stride);
		where += row_stride;
	}

	(void) jpeg_finish_decompress(&cinfo);

	process(image, cinfo.output_width, cinfo.output_height, cinfo.output_components, filename,
		0, 0, cinfo.output_width, cinfo.output_height, prefix);
	free(image);

	jpeg_destroy_decompress(&cinfo);
	fclose(infile);
}

int main(int argc, char **argv) {
	char s[2000];
	int i;

	while (fgets(s, 2000, stdin)) {
		s[strlen(s) - 1] = '\0';
		read_JPEG_file(s, argv[1]);
	}
}

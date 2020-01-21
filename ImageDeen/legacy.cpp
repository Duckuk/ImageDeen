#include <iostream>
#include "Clmg.h"

using namespace cimg_library;
using namespace std;

void encodeImageLegacy(CImg<unsigned char> image) {

	char hexString[7];
	unsigned int r, g, b;

	image.resize(image.width() + 1, image.height(), image.depth(), image.spectrum(), 0);

	CImgList<unsigned char> imageList = image.get_split('y');

	cimglist_for(imageList, l) {

		sprintf_s(hexString, "%06X", l);
		sscanf_s(hexString, "%2x%2x%2x", &r, &b, &g); //0xRRBBGG

		imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 0) = r;
		imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 1) = g;
		imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 2) = b;

		cout << hexString << endl;
	}

	random_shuffle(imageList.begin(), imageList.end());

	CImg<unsigned char> newImage(imageList.get_append('y'));

	newImage.save_png("output_e.png");
	cout << "Wrote to 'output_e.png'" << std::endl;
}

void decodeImageLegacy(CImg<unsigned char> image) {

	char redHex[3];
	char greenHex[3];
	char blueHex[3];

	CImgList<unsigned char> imageList = image.get_split('y');

	for (int i = 0; i < 10; i++) { //Do 10 passes
		cimglist_for(imageList, l) {

			int redValue = imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 0);
			int greenValue = imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 1);
			int blueValue = imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 2);

			sprintf_s(redHex, "%02X", redValue);
			sprintf_s(greenHex, "%02X", greenValue);
			sprintf_s(blueHex, "%02X", blueValue);

			string hex = string(redHex) + string(blueHex) + string(greenHex);
			long int decodedIndex = strtol(hex.c_str(), NULL, 16);

			if (imageList[l] == decodedIndex) {
				continue;
			}
			imageList[l].swap(imageList[decodedIndex]);
		}
		cout << "Pass: " << i + 1 << endl;
	}

	CImg<unsigned char> newImage(imageList.get_append('y'));
	newImage.resize(image.width() - 1, image.height(), image.depth(), image.spectrum(), 0);

	newImage.save_png("output_d.png");
	cout << "Wrote to 'output_d.png'" << endl;
}

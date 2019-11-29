#include <iostream>
#include <ostream>
#include <cctype>
#include <cstring>
#include <string>
#include <chrono>
#include <thread>
#include "libpng/png.h"
#include "Clmg.h"
using namespace cimg_library;
using namespace std;

void encodeImage(CImg<unsigned char> image, CImgDisplay &main_disp) {

	char hexString[7];
	unsigned int r, g, b;

	//Resize image to make room for index row
	image.resize(image.width() + 1, image.height(), image.depth(), image.spectrum(), 0);

	CImgList<unsigned char> imageList = image.get_split('y');

	for (int i = 0; i < 2; i++) {

		//Do X axis when we're done with the Y axis
		if (i > 0) {
			//Resize image to make room for index column
			image.resize(image.width(), image.height() + 1, image.depth(), image.spectrum(), 0);
			imageList = image.get_split('x');
		}

		cimglist_for(imageList, l) {

			//Convert index number to 6-character hexadecimal and split into 3
			sprintf_s(hexString, "%06X", l);
			sscanf_s(hexString, "%2x%2x%2x", &r, &b, &g); //0xRRBBGG

			//Set index RGB values and make transparant
			imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 0) = r;
			imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 1) = g;
			imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 2) = b;
			imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 3) = 0;

			cout << hexString << endl;
		}

		random_shuffle(imageList.begin(), imageList.end());

		main_disp.display(imageList, i > 0 ? 'x' : 'y');
		
		if (i > 0) {
			image = imageList.get_append('x');
		}
		else {
			image = imageList.get_append('y');
		}
	}

	image.save_png("output_e.png");
	cout << "Wrote to 'output_e.png'" << endl;
}

void decodeImage(CImg<unsigned char> image, CImgDisplay &main_disp) {

	char redHex[3];
	char greenHex[3];
	char blueHex[3];

	CImgList<unsigned char> imageList = image.get_split('x');

	for (int ia = 0; ia < 2; ia++) {

		//Do Y axis when we're done with the X axis
		if (ia > 0) {
			imageList = image.get_split('y');
		}

		for (int i = 0; i < 10; i++) { //Do 10 passes just in case
			cimglist_for(imageList, l) {

				//Store RGB values of the last pixel
				int redValue = imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 0);
				int greenValue = imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 1);
				int blueValue = imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 2);

				//Convert RGB values to hexadecimal
				sprintf_s(redHex, "%02X", redValue);
				sprintf_s(greenHex, "%02X", greenValue);
				sprintf_s(blueHex, "%02X", blueValue);

				//Combine hexadecimals and convert back to decimal
				string hex = string(redHex) + string(blueHex) + string(greenHex);
				long int decodedIndex = strtol(hex.c_str(), NULL, 16);

				if (imageList[l] == decodedIndex) {
					continue;
				}
				imageList[l].swap(imageList[decodedIndex]);

			}
			cout << "Pass: " << i+1 << endl;
		}

		if (ia > 0) {
			//Piece it together and cut off the index column
			image = imageList.get_append('y');
			image.resize(image.width() - 1, image.height(), image.depth(), image.spectrum(), 0);
			cout << "Y axis done\n" << endl;
		}
		else {
			//Piece it together and cut off the index row
			image = imageList.get_append('x');
			image.resize(image.width(), image.height() - 1, image.depth(), image.spectrum(), 0);
			cout << "X axis done\n" << endl;
		}

		main_disp.display(image);
	}

	image.save_png("output_d.png");
	cout << "Wrote to 'output_d.png'" << endl;
}

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

	for (int i=0; i<10; i++) { //Do 10 passes
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
		cout << "Pass: " << i+1 << endl;
	}

	CImg<unsigned char> newImage(imageList.get_append('y'));
	newImage.resize(image.width()-1, image.height(), image.depth(), image.spectrum(), 0);

	newImage.save_png("output_d.png");
	cout << "Wrote to 'output_d.png'" << endl;
}

int main(int argc, char *argv[]) {

	if (argc < 2) {
		MessageBoxA(NULL, "Image file not specified.", NULL, MB_OK | MB_ICONERROR);
		return 0;
	}
	
	srand(unsigned (time(NULL)));

	CImg<unsigned char> image(argv[1]);

	bool flag = false;

	if (image.spectrum() < 4) {
		flag = true;
	}
	if (image.spectrum() < 3) {
		image.resize(image.width(), image.height(), image.depth(), 3);
	}

	CImgDisplay main_disp(image, "Image", NULL, NULL, true);
	while (main_disp.width() > 800 || main_disp.height() > 800) {
		main_disp.resize(main_disp.width() / 2, main_disp.height() / 2, true);
	}

	main_disp.show();

	char axis;
	cout << "Pick mode:" << endl
		<< " E (Encode)" << endl
		<< " D (Decode)" << endl
		<< " N (Legacy Encode)" << endl
		<< " C (Legacy Decode)" << endl;
	cin >> axis;
	cin.ignore();

	axis = tolower(axis);

	switch (axis) {
		case 'e':
			image.channels(0, 3);
			if (flag) image.get_shared_channel(3).fill(255);
			encodeImage(image, main_disp);
			break;
		case 'd':
			image.channels(0, 3);
			decodeImage(image, main_disp);
			break;
		case 'n':
			encodeImageLegacy(image);
			break;
		case 'c':
			decodeImageLegacy(image);
			break;
	}
	this_thread::sleep_for(std::chrono::seconds(2));
	return 1;
}
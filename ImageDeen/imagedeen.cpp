#include <cctype>
#include <chrono>
#include <conio.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include "Clmg.h"
#include "CRC.h"
#include "legacy.cpp"
#include "png.h"

using std::cout;
using std::endl;
using namespace cimg_library;

void encodeImage_key(CImg<unsigned char> image, CImgDisplay& main_disp, unsigned int checksum) {

	unsigned int r, g, b,
		rC, gC, bC, aC;
	char hexString[7], hexChecksum[9],
		fileName[256];

	//Convert checksum number to 8-character hexadecimal and split into 4
	sprintf_s(hexChecksum, "%08X", checksum);
	sscanf_s(hexChecksum, "%2x%2x%2x%2x", &aC, &rC, &gC, &bC);

	//Resize image to make room for index column
	image.resize(image.width() + 1, image.height(), image.depth(), image.spectrum(), 0);

	CImgList<unsigned char> imageList = image.get_split('y');

	for (int i = 0; i < 2; i++) {

		//Do X axis when we're done with the Y axis
		if (i > 0) {
			//Resize image to make room for index and checksum rows
			image.resize(image.width(), image.height() + 2, image.depth(), image.spectrum(), 0);
			imageList = image.get_split('x');
		}

		cimglist_for(imageList, l) {

			//Convert index number to 6-character hexadecimal and split into 3
			sprintf_s(hexString, "%06X", l);
			sscanf_s(hexString, "%2x%2x%2x", &r, &b, &g); //0xRRBBGG

			//Set index RGB values and make transparant
			imageList[l](imageList[l].width() - 1, imageList[l].height() - (1+i), image.depth() - 1, 0) = r;
			imageList[l](imageList[l].width() - 1, imageList[l].height() - (1+i), image.depth() - 1, 1) = g;
			imageList[l](imageList[l].width() - 1, imageList[l].height() - (1+i), image.depth() - 1, 2) = b;
			imageList[l](imageList[l].width() - 1, imageList[l].height() - (1+i), image.depth() - 1, 3) = 0;

			if (i > 0) {
				//Set checksum RGBA values
				imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 0) = rC;
				imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 1) = gC;
				imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 2) = bC;
				imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 3) = aC;
			}
			cout << hexString << endl;
		}

		random_shuffle(imageList.begin(), imageList.end());

		main_disp.display(imageList, i > 0 ? 'x' : 'y');

		if (i > 0) {
			image = imageList.get_append('x');
			image(0, image.height() - 1, image.depth() - 1, 3);
		}
		else {
			image = imageList.get_append('y');
		}
	}
	
	sprintf(fileName, "output_e_%d.png", int(time(NULL)));

	image.save_png(fileName);
	cout << "Wrote to '" << fileName << '\'' << endl;
}

void decodeImage_key(CImg<unsigned char> image, CImgDisplay& main_disp, unsigned int checksum) {

	unsigned long decodedChecksum;
	int redValue, greenValue, blueValue,
		checkRedValue, checkGreenValue, checkBlueValue, checkAlphaValue;
	char redHex[3], greenHex[3], blueHex[3],
		checkRedHex[3], checkGreenHex[3], checkBlueHex[3], checkAlphaHex[3],
		fileName[256];
	string hex;

	CImgList<unsigned char> imageList = image.get_split('x');

	for (int ia = 0; ia < 2; ia++) {

		//Do Y axis when we're done with the X axis
		if (ia > 0) {
			imageList = image.get_split('y');
		}

		for (int i = 0; i < 10; i++) { //Do 10 passes just in case
			cimglist_for(imageList, l) {

				if (!ia) {
					//Store RGBA values of the checksum pixel
					checkRedValue = imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 0);
					checkGreenValue = imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 1);
					checkBlueValue = imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 2);
					checkAlphaValue = imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 3);

					//Convert checksum RGBA values to hexadecimal
					sprintf_s(checkRedHex, "%02X", checkRedValue);
					sprintf_s(checkGreenHex, "%02X", checkGreenValue);
					sprintf_s(checkBlueHex, "%02X", checkBlueValue);
					sprintf_s(checkAlphaHex, "%02X", checkAlphaValue);

					//Combine index hexadecimals and convert back to decimal
					hex = string(checkAlphaHex) + string(checkRedHex) + string(checkGreenHex) + string(checkBlueHex);
					decodedChecksum = strtoul(hex.c_str(), NULL, 16);

					if (decodedChecksum != checksum) {
						cerr << "Error: Mismatched checksum" << endl;
						return;
					}
				}

				//Store RGB values of the index pixel
				redValue = imageList[l](imageList[l].width() - 1, imageList[l].height() - (2 - ia), image.depth() - 1, 0);
				greenValue = imageList[l](imageList[l].width() - 1, imageList[l].height() - (2 - ia), image.depth() - 1, 1);
				blueValue = imageList[l](imageList[l].width() - 1, imageList[l].height() - (2 - ia), image.depth() - 1, 2);

				//Convert index RGB values to hexadecimal
				sprintf_s(redHex, "%02X", redValue);
				sprintf_s(greenHex, "%02X", greenValue);
				sprintf_s(blueHex, "%02X", blueValue);

				//Combine index hexadecimals and convert back to decimal
				hex = string(redHex) + string(blueHex) + string(greenHex);
				long int decodedIndex = strtol(hex.c_str(), NULL, 16);

				if (imageList[l] == decodedIndex) {
					continue;
				}
				imageList[l].swap(imageList[decodedIndex]);

			}
			cout << "Pass: " << i + 1 << endl;
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
			image.resize(image.width(), image.height() - 2, image.depth(), image.spectrum(), 0);
			cout << "X axis done\n" << endl;
		}

		main_disp.display(image);
	}
	
	sprintf(fileName, "output_d_%d.png", int(time(NULL)));

	image.save_png(fileName);
	cout << "Wrote to '" << fileName << '\'' << endl;
}

void encodeImage(CImg<unsigned char> image, CImgDisplay &main_disp) {

	char hexString[7],
		fileName[256];
	unsigned int r, g, b;

	//Resize image to make room for index column
	image.resize(image.width() + 1, image.height(), image.depth(), image.spectrum(), 0);

	CImgList<unsigned char> imageList = image.get_split('y');

	for (int i = 0; i < 2; i++) {

		//Do X axis when we're done with the Y axis
		if (i > 0) {
			//Resize image to make room for index row
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

	sprintf(fileName, "output_e_%d.png", int(time(NULL)));

	image.save_png(fileName);
	cout << "Wrote to '" << fileName << '\'' << endl;
}

void decodeImage(CImg<unsigned char> image, CImgDisplay &main_disp) {

	char redHex[3], greenHex[3], blueHex[3],
		fileName[256];
	int redValue, greenValue, blueValue;
	string hex;
	unsigned long decodedIndex;

	CImgList<unsigned char> imageList = image.get_split('x');

	for (int ia = 0; ia < 2; ia++) {

		//Do Y axis when we're done with the X axis
		if (ia > 0) {
			imageList = image.get_split('y');
		}

		for (int i = 0; i < 10; i++) { //Do 10 passes just in case
			cimglist_for(imageList, l) {

				//Store RGB values of the index pixel
				redValue = imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 0);
				greenValue = imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 1);
				blueValue = imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 2);

				//Convert RGB values to hexadecimal
				sprintf_s(redHex, "%02X", redValue);
				sprintf_s(greenHex, "%02X", greenValue);
				sprintf_s(blueHex, "%02X", blueValue);

				//Combine hexadecimals and convert back to decimal
				hex = string(redHex) + string(blueHex) + string(greenHex);
				decodedIndex = strtoul(hex.c_str(), NULL, 16);

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

	sprintf(fileName, "output_d_%d.png", int(time(NULL)));

	image.save_png(fileName);
	cout << "Wrote to '" << fileName << '\'' << endl;
}

int main(int argc, char *argv[]) {

	if (argc < 2) {
		MessageBoxA(NULL, "Image file not specified.", NULL, MB_OK | MB_ICONERROR);
		return 1;
	}
	
	bool exit = false;
	bool flag = false;
	char mode;
	CImg<unsigned char> image(argv[1]);
	string key;
	unsigned int checksum;

	srand(unsigned (time(NULL)));

	fstream keyFile("ImageDeenKey.txt", ios::in);
	ifstream exeFile("ImageDeen.exe");

	if (!keyFile.good() && exeFile.good()) {
		exeFile.close();
		keyFile.close(); keyFile.clear();
		keyFile.open("ImageDeenKey.txt", ios::out);
		keyFile << "PUT_YOUR_KEY_HERE" << endl;
		keyFile.close(); keyFile.clear();
		keyFile.open("ImageDeenKey.txt", ios::in);
	}

	if (exeFile.good())
		getline(keyFile, key);

	checksum = CRC::Calculate(key.c_str(), key.length(), CRC::CRC_32());

	if (key == "PUT_YOUR_KEY_HERE") {
		cout << "Please set your key in ImageDeenKey.txt" << endl;
	}

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

	cout << "Pick mode:" << endl
		<< " E (Encode)" << endl
		<< " D (Decode)" << endl
		<< " N (Legacy Encode)" << endl
		<< " C (Legacy Decode)" << endl;
	
	while (!exit) {
		switch (tolower(_getch())) {
			case 'e':
				image.channels(0, 3);
				if (flag) image.get_shared_channel(3).fill(255);
				if (keyFile.good()) {
					cout << "Use Key (Y/N): ";
					if (tolower(_getch()) == 'y') {
						encodeImage_key(image, main_disp, checksum);
						break;
					}
				}
				encodeImage(image, main_disp);
				exit = true;
				break;
			case 'd':
				image.channels(0, 3);
				if (keyFile.good()) {
					cout << "Use Key (Y/N): ";
					if (tolower(_getch()) == 'y') {
						decodeImage_key(image, main_disp, checksum);
						break;
					}
				}
				decodeImage(image, main_disp);
				exit = true;
				break;
			case 'n':
				encodeImageLegacy(image);
				exit = true;
				break;
			case 'c':
				decodeImageLegacy(image);
				exit = true;
				break;
			default:
				cerr << "Error: Unknown mode" << endl;
		}
	}
	this_thread::sleep_for(std::chrono::seconds(2));
	return 0;
}

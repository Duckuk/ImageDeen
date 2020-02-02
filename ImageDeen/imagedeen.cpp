#define PDC_WIDE

#include <chrono>
#include <fstream>
#include <string>
#include <thread>
#include "Clmg.h"
#include "CRC.h"
#include "curses.h"
#include "legacy.cpp"

using namespace cimg_library;


int TERM_X = 96;
int TERM_Y = 20;

WINDOW *log_border, *log_win;
WINDOW *mode_border, *mode_win;
WINDOW *hotkey_border, *hotkey_win;
WINDOW *info_border, *info_win;

enum EncodeTypes {
	NONE = -1,
	ENCODE,
	DECODE,
	ENCODE_LEGACY,
	DECODE_LEGACY
};

void writeLog(const char * message) {
	wprintw(log_win, "\n%s", message);
	wrefresh(log_win);
}

void displayImage(CImgDisplay &main_disp, CImg<unsigned char> image) {
	main_disp.display(image.get_resize(main_disp.width(), main_disp.height(), image.depth(), image.spectrum(), 3));
	if (main_disp.is_closed())
		main_disp.show();
	writeLog("Preview image ready.");
}

void setTitle(WINDOW* border_win, const char * text, bool faded = false) {
	const chtype bar = L'\u2588';
	if (!faded) {
		wattron(border_win, A_REVERSE | A_BOLD);
		mvwprintw(border_win, 0, (getmaxx(border_win) / 2) - (strlen(text) / 2), text);
		wattroff(border_win, A_REVERSE | A_BOLD);
	}
	else {
		wattron(border_win, A_REVERSE | A_BLINK);
		mvwprintw(border_win, 0, (getmaxx(border_win) / 2) - (strlen(text) / 2), text);
		wattroff(border_win, A_REVERSE | A_BLINK);
	}
	wrefresh(border_win);
}

void encodeImage_key(CImg<unsigned char> image, CImgDisplay& main_disp, unsigned int checksum) {

	unsigned int r, g, b,
		rC, gC, bC, aC;
	char hexString[7], hexChecksum[9],
		fileName[256];
	float progress = 0;

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

		waddch(log_win, '\n');

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

			if (i > 0) {
				wclrtoeol(log_win);
				wmove(log_win, getcury(log_win), 0);
				wprintw(log_win, "2nd Pass: %.1f%%", (float)(image.width() - (image.width() - l - 1)) / (float)image.width() * 100);
			}
			else {
				wclrtoeol(log_win);
				wmove(log_win, getcury(log_win), 0);
				wprintw(log_win, "1st Pass: %.1f%%", (float)(image.height() - (image.height() - l - 1)) / (float)image.height() * 100);
			}
			wrefresh(log_win);
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
	wprintw(log_win, "\nWrote to \'%s\'", fileName);
	wrefresh(log_win);
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
						wattron(log_win, A_BOLD | A_BLINK);
						writeLog("Error: Mismatched checksum");
						wattroff(log_win, A_BOLD | A_BLINK);
						wrefresh(log_win);
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
			wprintw(log_win, "Pass: %i\n", i + 1);
			wrefresh(log_win);
		}

		if (ia > 0) {
			//Piece it together and cut off the index column
			image = imageList.get_append('y');
			image.resize(image.width() - 1, image.height(), image.depth(), image.spectrum(), 0);
			writeLog("Y axis done\n");
			main_disp.display(image.get_resize(main_disp.width(), main_disp.height(), image.depth(), image.spectrum(), 3));
		}
		else {
			//Piece it together and cut off the index row
			image = imageList.get_append('x');
			image.resize(image.width(), image.height() - 2, image.depth(), image.spectrum(), 0);
			writeLog("X axis done\n");
			main_disp.display(image);
		}
		wrefresh(log_win);
	}
	
	sprintf(fileName, "output_d_%d.png", int(time(NULL)));

	image.save_png(fileName);
	wprintw(log_win, "\nWrote to \'%s\'", fileName);
	wrefresh(log_win);
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

		waddch(log_win, '\n');

		cimglist_for(imageList, l) {

			//Convert index number to 6-character hexadecimal and split into 3
			sprintf_s(hexString, "%06X", l);
			sscanf_s(hexString, "%2x%2x%2x", &r, &b, &g); //0xRRBBGG

			//Set index RGB values and make transparant
			imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 0) = r;
			imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 1) = g;
			imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 2) = b;
			imageList[l](imageList[l].width() - 1, imageList[l].height() - 1, image.depth() - 1, 3) = 0;
			
			if (i > 0) {
				wclrtoeol(log_win);
				wmove(log_win, getcury(log_win), 0);
				wprintw(log_win, "2nd Pass: %.1f%%", (float)(image.width() - (image.width() - l - 1)) / (float)image.width() * 100);
			}
			else {
				wclrtoeol(log_win);
				wmove(log_win, getcury(log_win), 0);
				wprintw(log_win, "1st Pass: %.1f%%", (float)(image.height() - (image.height() - l - 1)) / (float)image.height() * 100);
			}
			wrefresh(log_win);
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
	wprintw(log_win, "\nWrote to \'%s\'", fileName);
	wrefresh(log_win);
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
			wprintw(log_win, "Pass: %i\n", i+1);
			wrefresh(log_win);
		}

		if (ia > 0) {
			//Piece it together and cut off the index column
			image = imageList.get_append('y');
			image.resize(image.width() - 1, image.height(), image.depth(), image.spectrum(), 0);
			writeLog("Y axis done\n");
			main_disp.display(image.get_resize(main_disp.width(), main_disp.height(), image.depth(), image.spectrum(), 3));
		}
		else {
			//Piece it together and cut off the index row
			image = imageList.get_append('x');
			image.resize(image.width(), image.height() - 1, image.depth(), image.spectrum(), 0);
			writeLog("X axis done\n");
			main_disp.display(image);
		}
		wrefresh(log_win);
	}

	sprintf(fileName, "output_d_%d.png", int(time(NULL)));

	image.save_png(fileName);
	wprintw(log_win, "\nWrote to \'%s\'", fileName);
	wrefresh(log_win);
}

int main(int argc, char *argv[]) {

	if (argc < 2) {
		MessageBoxA(NULL, "Image file not specified.", NULL, MB_OK | MB_ICONERROR);
		return 1;
	}

	SetConsoleTitle((LPCSTR)"ImageDeen");

	//Initalize curses & set settings
	initscr();
	cbreak();
	noecho();
	resize_term(TERM_Y, TERM_X);
	curs_set(0);

	//Initialize log window w/ loading message
	log_border = newwin(16, TERM_X-23, 0, 23);
	wborder(log_border,
		L'\u2588' | A_BOLD,	//Left side
		L'\u2588' | A_BOLD,	//Right side
		L'\u2580' | A_BOLD,	//Top side
		' ' | A_BOLD,	//Bottom side
		L'\u2588' | A_BOLD,	//Top left corner
		L'\u2588' | A_BOLD,	//Top right corner
		L'\u2588' | A_BOLD,	//Bottom left corner
		L'\u2588' | A_BOLD	//Bottom right corner
	);
	setTitle(log_border, "Log");
	wrefresh(log_border);
	log_win = derwin(log_border, getmaxy(log_border)-2, getmaxx(log_border)-4, 1, 2);
	scrollok(log_win, true);
	wmove(log_win, getmaxy(log_win)-1, 0);
	wrefresh(log_win);

	writeLog("ImageDeen is now preparing.\nPlease watch warmly until it is ready.\n");

	//Initalize modes window
	mode_border = newwin(7, 24, 0, 0);
	wborder( mode_border,
		L'\u2588' | A_BOLD,	//Left side
		L'\u2588' | A_BOLD,	//Right side
		L'\u2580' | A_BOLD,	//Top side
		' ' | A_BOLD,	//Bottom side
		L'\u2588' | A_BOLD,	//Top left corner
		L'\u2588' | A_BOLD,	//Top right corner
		L'\u2588' | A_BOLD,	//Bottom left corner
		L'\u2588' | A_BOLD	//Bottom right corner
	);
	setTitle(mode_border, "Mode");
	wrefresh(mode_border);
	mode_win = derwin(mode_border, getmaxy(mode_border)-1, getmaxx(mode_border)-4, 1, 2);
	scrollok(mode_win, true);
	
	//Initalize hotkeys window
	hotkey_border = newwin(9, 24, getmaxy(mode_border), 0);
	wborder(hotkey_border,
		L'\u2588' | A_BOLD,	//Left side
		L'\u2588' | A_BOLD,	//Right side
		L'\u2580' | A_BOLD,	//Top side
		' ' | A_BOLD,	//Bottom side
		L'\u2588' | A_BOLD,	//Top left corner
		L'\u2588' | A_BOLD,	//Top right corner
		L'\u2588' | A_BOLD,	//Bottom left corner
		L'\u2588' | A_BOLD	//Bottom right corner
	);
	setTitle(hotkey_border, "Hotkeys");
	wrefresh(hotkey_border);
	hotkey_win = derwin(hotkey_border, getmaxy(hotkey_border)-1, getmaxx(hotkey_border)-4, 1, 2);
	scrollok(hotkey_win, true);

	//Initialize info window
	info_border = newwin(4, TERM_X, 16, 0);
	wborder(info_border,
		L'\u2588' | A_BOLD,	//Left side
		L'\u2588' | A_BOLD,	//Right side
		L'\u2580' | A_BOLD,	//Top side
		L'\u2584' | A_BOLD,	//Bottom side
		L'\u2588' | A_BOLD,	//Top left corner
		L'\u2588' | A_BOLD,	//Top right corner
		L'\u2588' | A_BOLD,	//Bottom left corner
		L'\u2588' | A_BOLD	//Bottom right corner
	);
	setTitle(info_border, "Info");
	wrefresh(info_border);
	info_win = derwin(info_border, getmaxy(info_border)-1, getmaxx(info_border)-4, 1, 2);
	wrefresh(info_win);

	//'Important' variable/constant declarations
	bool exit = false,
		flag = false,
		keyed = true,
		beginReady = false,
		keyReady = false;

	const char * modes[4] = {
		"E (Encode)",
		"D (Decode)",
		"N (Legacy Encode)",
		"C (Legacy Decode)"
	};
	const char * hotkeys[2] = {
		"ENTER (Begin)",
		"K (Key)"
	};

	EncodeTypes modeSelected = NONE;

	string fileName = argv[1]; fileName = fileName.substr(fileName.rfind('\\') + 1).c_str();
	string key;

	wprintw(log_win, "\nAttempting to open \'%s\'...", fileName.c_str());
	CImg<unsigned char> image(argv[1]);
	writeLog("Done!");

	int dimensions[2] = { image.width(), image.height() };

	unsigned int checksum;
	
	//Seed RNG
	srand(unsigned (time(NULL)));

	//Open key file (also exe for anti-bloat purposes)
	fstream keyFile("ImageDeenKey.txt", ios::in);
	ifstream exeFile("ImageDeen.exe");

	if (!keyFile.good() && exeFile.good()) {
		exeFile.close();
		keyFile.close(); keyFile.clear();
		keyFile.open("ImageDeenKey.txt", ios::out);
		keyFile << "PUT_YOUR_KEY_HERE" << std::endl;
		keyFile.close(); keyFile.clear();
		keyFile.open("ImageDeenKey.txt", ios::in);
	}

	if (exeFile.good())
		getline(keyFile, key);
	if (!keyFile.good()) {
		writeLog("\nKeying will be unavailable due to either intentional disabling or a missing ImageDeenKey.txt");
		keyed = false;
	}
	else if (key == "PUT_YOUR_KEY_HERE") {
		keyed = false;
	}
	else {
		checksum = CRC::Calculate(key.c_str(), key.length(), CRC::CRC_32());
	}

	//Print info
	wprintw(info_win, "\tWidth:\t%d\t\tChannels:\t%d\t\tColour Space:\t%s\n"
					  "\tHeight:\t%d\t\tDepth:\t\t%d\t\tRAM Size:\t%dkb", 
		dimensions[0], image.spectrum(), (image.spectrum()==1) ? "MONO" : (image.spectrum()==2) ? "BW&A" : (image.spectrum()==3) ? "RGB" : (image.spectrum()==4) ? "RGBA" : "Unknown", 
		dimensions[1], image.depth(), image.size()*sizeof(unsigned char)/1024);
	wrefresh(info_win);
	

	//Resize image to RGBA if monochrome
	if (image.spectrum() == 1) {
		image.resize(image.width(), image.height(), image.depth(), 4);
	}
	else if (image.spectrum() == 2) {
		image.resize(image.width(), image.height(), image.depth(), 4);
		image.get_shared_channel(1) = image.get_channel(0);
		image.get_shared_channel(2) = image.get_channel(0);
	}

	//Flag image for alpha channel filling if encode is picked and image isn't RGBA
	if (image.spectrum() < 4) {
		flag = true;
	}
	
	//Calculate dimensions for preview image
	while (dimensions[0] > 800 || dimensions[1] > 800) {
		dimensions[0] = dimensions[0] / 2;
		dimensions[1] = dimensions[1] / 2;
	}

	//Initalize preview image display and display image using advanced 'Clausewitz doesn't multithread' technology (multithreading)
	CImgDisplay main_disp(dimensions[0], dimensions[1], fileName.substr(0, fileName.rfind('.')).c_str(), NULL, false, true);
	thread displayImageThread(displayImage, ref(main_disp), image);

	//Print mode & hotkey options
	mvwprintw(mode_win, 1, 0, "%s\n%s\n%s\n%s\n", modes[0], modes[1], modes[2], modes[3]);
	wrefresh(mode_win);

	mvwprintw(hotkey_win, 1, 0, "%s\n%s", hotkeys[0], hotkeys[1]);
	wrefresh(hotkey_win);
	
	if (keyed) {
		wattron(log_border, A_REVERSE | A_BOLD);
		mvwprintw(log_border, 0, 1, "Key|True ");
		wattroff(log_border, A_REVERSE | A_BOLD);
	}
	else {
		wattron(log_border, A_REVERSE | A_BOLD);
		mvwprintw(log_border, 0, 1, "Key|False");
		wattroff(log_border, A_REVERSE | A_BOLD);
	}
	wrefresh(log_border);
	
	//Print ready message & bark at user to set their key
	writeLog("\nReady!");

	if (key == "PUT_YOUR_KEY_HERE") {
		wattron(log_win, A_BOLD | A_BLINK);
		writeLog("Please set your key in ImageDeenKey.txt");
		wattroff(log_win, A_BOLD | A_BLINK);
	}
	
	while (!exit) {

		if (modeSelected == NONE) {
			beginReady = false;
			mvwchgat(hotkey_win, 1, 0, strlen(hotkeys[0]), A_BLINK | A_BOLD, NULL, NULL);
		}
		else {
			beginReady = true;
			mvwchgat(hotkey_win, 1, 0, strlen(hotkeys[0]), A_NORMAL, NULL, NULL);
		}

		//Check conditions for keying and gr[ae]y out if applicable
		if (modeSelected == NONE || modeSelected == ENCODE_LEGACY || modeSelected == DECODE_LEGACY || !keyFile.good() || !keyed) {
			keyReady = false;
			mvwchgat(hotkey_win, 2, 0, strlen(hotkeys[1]), A_BLINK | A_BOLD, NULL, NULL);
		}
		else {
			keyReady = true;
			mvwchgat(hotkey_win, 2, 0, strlen(hotkeys[1]), A_NORMAL, NULL, NULL);
		}
		wrefresh(hotkey_win);

		switch (tolower(getch())) {

			//Switch to and/or toggle encode
			case 'e':
				if (modeSelected != NONE)
					mvwchgat(mode_win, modeSelected + 1, 0, -1, A_NORMAL, NULL, NULL);
				if (modeSelected != ENCODE) {
					mvwchgat(mode_win, ENCODE + 1, 0, strlen(modes[ENCODE]), A_STANDOUT, NULL, NULL);
					modeSelected = ENCODE;
				}
				else {
					modeSelected = NONE;
				}
				wrefresh(mode_win);
				break;
			
			//Switch to and/or toggle decode
			case 'd':
				if (modeSelected != NONE)
					mvwchgat(mode_win, modeSelected + 1, 0, -1, A_NORMAL, NULL, NULL);
				if (modeSelected != DECODE) {
					mvwchgat(mode_win, DECODE + 1, 0, strlen(modes[DECODE]), A_STANDOUT, NULL, NULL);
					modeSelected = DECODE;
				}
				else {
					modeSelected = NONE;
				}
				wrefresh(mode_win);
				break;

			//Switch to and/or toggle legacy encode
			case 'n':
				if (modeSelected != NONE)
					mvwchgat(mode_win, modeSelected + 1, 0, -1, A_NORMAL, NULL, NULL);
				if (modeSelected != ENCODE_LEGACY) {
					mvwchgat(mode_win, ENCODE_LEGACY + 1, 0, strlen(modes[ENCODE_LEGACY]), A_STANDOUT, NULL, NULL);
					modeSelected = ENCODE_LEGACY;
				}
				else {
					modeSelected = NONE;
				}
				wrefresh(mode_win);
				break;

			//Switch to and/or toggle legacy decode
			case 'c':
				if (modeSelected != NONE)
					mvwchgat(mode_win, modeSelected + 1, 0, -1, A_NORMAL, NULL, NULL);
				if (modeSelected != DECODE_LEGACY) {
					mvwchgat(mode_win, DECODE_LEGACY + 1, 0, strlen(modes[DECODE_LEGACY]), A_STANDOUT, NULL, NULL);
					modeSelected = DECODE_LEGACY;
				}
				else {
					modeSelected = NONE;
				}
				wrefresh(mode_win);
				break;

			//Execute order D33N
			case '\n':
				if (!beginReady)
					break;

				exit = true;
				
				//Gr[ae]y out mode & hotkey windows
				for (int i = 0; i < getmaxy(mode_border); i++) {
					wmove(mode_border, i, 0);
					wchgat(mode_border, -1, A_BLINK, NULL, NULL);
				}
				setTitle(mode_border, "Mode", true);
				wrefresh(mode_border);

				for (int i = 0; i < getmaxy(hotkey_border); i++) {
					wmove(hotkey_border, i, 0);
					wchgat(hotkey_border, -1, A_BLINK, NULL, NULL);
				}
				setTitle(hotkey_border, "Hotkeys", true);
				wrefresh(hotkey_border);

				switch (modeSelected) {
					case ENCODE:
						image.channels(0, 3);
						if (flag) image.get_shared_channel(3).fill(255);
						if (displayImageThread.joinable())
							displayImageThread.join();
						if (keyed)
							encodeImage_key(image, main_disp, checksum);
						else
							encodeImage(image, main_disp);
						break;
					case DECODE:
						image.channels(0, 3);
						if (displayImageThread.joinable())
							displayImageThread.join();
						if (keyed)
							decodeImage_key(image, main_disp, checksum);
						else
							decodeImage(image, main_disp);
						break;
					case ENCODE_LEGACY:
						endwin();
						encodeImageLegacy(image);
						this_thread::sleep_for(std::chrono::seconds(2));
						return 0;
					case DECODE_LEGACY:
						endwin();
						decodeImageLegacy(image);
						this_thread::sleep_for(std::chrono::seconds(2));
						return 0;
					default:
						continue;
				}
				break;
			case 'k':
				if (!keyReady)
					break;
				if (keyed) {
					wattron(log_border, A_REVERSE | A_BOLD);
					mvwprintw(log_border, 0, 5, "False");
					wattroff(log_border, A_REVERSE | A_BOLD);
				}
				else {
					wattron(log_border, A_REVERSE | A_BOLD);
					mvwprintw(log_border, 0, 5, "True ");
					wattroff(log_border, A_REVERSE | A_BOLD);
				}
				wrefresh(log_border);
				keyed = !keyed;
				break;
			default:
			continue;
		}
	}
	
	this_thread::sleep_for(std::chrono::seconds(2));
	endwin();
	return 0;
}

// TMIDI by Tom Grandgent
// Started 4/19/99

#pragma warning( disable : 4996 )

/*

  Intelligence features:

  - Tries to figure out the title of a song
  - A MIDI standard logo is shown if a file does a reset for that standard (GM, GS, XG, MT-32)
  - Sysex is decoded for GS, XG, and MT-32
  - If a MIDI file is XG and it uses bank 127 for a channel, that channel's data is redirected to channel 10 (percussion)

 */

#define _WIN32_IE 0x0300	// Required for backwards compatibility with comctl32.dll in all Win32 apps!

#include <windows.h>
#include <mmsystem.h>
#include <commctrl.h>
#include <process.h>
#include <winsock.h>

#include <stdio.h>
#include <ctype.h>
#include <io.h>
#include <math.h>
#include <assert.h>

#include "TMIDI.h"
#include "resource.h"

// Function prototypes
// Registry functions
void read_registry_settings(void);
void write_registry_settings(void);
void check_associations(void);
void set_associations(void);
// File functions
int read_bytes(FILE *fp, unsigned char *buf, int num);
unsigned int read_int(FILE *fp);
unsigned short int read_short(FILE *fp);
unsigned int read_vlq(FILE *fp, unsigned int *offset);
// Memory functions
unsigned char read_byte_mem(track_header_t *th);
unsigned int read_int_mem(track_header_t *th);
int read_bytes_mem(track_header_t *th, unsigned char *buf, int num);
unsigned short int read_short_mem(track_header_t *th);
unsigned int read_vlq_mem(track_header_t *th);
// MIDI functions
void __cdecl playback_thread(void *spointer);
int load_midi(char *filename, HWND hDlg);
int analyze_midi(void);
int process_midi_event(track_header_t *th);
void reverse_endian_word(unsigned short int *word);
void enum_devices(HWND hwnd);
void init_midi_in(HWND hwndcb);
void init_midi_out(HWND hwndcb);
void close_midi_in(void);
void close_midi_out(void);
void CALLBACK MidiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);
void output_sysex_data(unsigned char channel, unsigned char *data, int length);
int handle_sysex_dump(FILE *fp);
void note_on(unsigned char on, unsigned char note, unsigned char velocity, unsigned char channel);
void all_notes_off_channel(int channel);
void all_notes_off(void);
void set_channel_program(int channel, int program, int bank);
void set_channel_controller(unsigned char channel, unsigned char controller, unsigned char value);
void set_program_override(int channel, int override, int program);
void update_note_volume(char channel, char note, char volume);
void update_display(HDC hdc);
void set_tempo(int new_tempo);
void set_mod_pitch(signed int i);
void set_mod_velocity(signed int i);
void set_channel_mute(int channel, int mute);
void set_channel_solo(int channel);
void init_mt32_state(void);
// Name resolution / sysex interpretation functions
char *get_drum_kit_name(int program);
char *get_sysex_manufacturer_name(int id);
char *interpret_sysex(unsigned char *s, int len);
char *interpret_model(unsigned char *s);
char *interpret_command(unsigned char *s);
char interpret_sysex_part(unsigned char c);
void interpret_mt32_patch_memory(unsigned char *s, int len);
void interpret_mt32_timbre_memory(unsigned char *s, int len);
char *get_yamaha_effect_name(unsigned char effect);
void check_midi_standard(unsigned char *data);
char *get_program_name(unsigned char program, unsigned char bank);
// Linked list functions
midi_text_t *new_midi_text(char *midi_text, int text_type, double midi_time, int track, int track_offset);
void kill_all_midi_text(void);
midi_sysex_t *new_midi_sysex(unsigned char *data, int length, double midi_time, int track, int track_offset, int channel);
void kill_all_midi_sysex(void);
// Playlist functions
playlist_t *playlist_add(char *filename);
void playlist_clear(void);
playlist_t *playlist_remove(playlist_t *target);
// Tracks list view functions
void InitTracksListView(HWND hwndLV);
void FillTracksListView(HWND hwndLV);
void SetupItemsTracksListView(HWND hwndLV);
// Channels list view functions
void InitChannelsListView(HWND hwndLV);
void FillChannelsListView(HWND hwndLV);
void SetupItemsChannelsListView(HWND hwndLV);
// Generic list view functions
void SetCachedLVItem(HWND hwndLV, int iItem, int column, signed int &lastval, int val);
void SetCachedLVItem(HWND hwndLV, int iItem, int column, signed int &lastval, char *str);
// Sysex list view functions
void InitSysexListView(HWND hwndLV);
void SetupItemsSysexListView(HWND hwndLV);
// Misc junk
void hsv_to_rgb_int(int h, int s, int v, int &r, int &g, int &b);
void hsv_to_rgb(float h, float s, float v, int *r, int *g, int *b);
int get_scroll_value(WPARAM wParam, LPARAM lParam, int *i);
char *stristr(char *src, char *target);
char *extract_filename(char *filename);
void copy_to_clipboard(char *str);
// Tooltip functions
LRESULT CALLBACK GetMsgProc(int nCode, WPARAM wParam, LPARAM lParam);
VOID OnWMNotify(LPARAM lParam);
WNDENUMPROC EnumChildProc(HWND hwndCtrl, LPARAM lParam);
BOOL DoCreateDialogTooltip(void);
void CleanupTooltip(void);
// User interface functions
void handle_mousedown(int x, int y, int button, int msg);
void handle_controller_bar_click(int x, int y, int channel);
int get_clicked_channel(int x, int y);
void handle_controller_bar_popup(int x, int y, int channel);
// GDI resource functions
void init_gdi_resources(void);
void free_gdi_resources(void);
// Dialog callback functions
INT_PTR CALLBACK MainDlg(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK TextDlg(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK TracksDlg(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ChannelsDlg(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK SysexDlg(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK AssocDlg(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK GenericTextDlg(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK OutConfigDlg(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);

WNDPROC OldButtonProc;
LRESULT CALLBACK NewButtonProc(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
				   PSTR szCmdLine, int iCmdShow)
{
	DWORD disposition;
	HWND hwnd;
	COPYDATASTRUCT cds;
	char fn[MAX_PATH];
	char *fnptr;

	ghInstance = hInstance;

	// Check to see if we're being called with a filename parameter
	if (szCmdLine[0] && szCmdLine[0] != '/')
	{
		strncpy(fn, szCmdLine, sizeof(fn));
		if (fn[0] == '\"')
			fnptr = fn + 1;
		else
			fnptr = fn;
		if (fnptr[strlen(fnptr) - 1] == '\"')
			fnptr[strlen(fnptr) - 1] = '\0';
		// Check to see if another instance of the program is running
		hwnd = FindWindow(NULL, "TMIDI: Tom's MIDI Player");
		cds.dwData = IPC_PLAY;
		cds.cbData = strlen(fnptr) + 1;
		cds.lpData = fnptr;
		if (hwnd)
		{
			// If so, tell that instance to play the specified file
			SendMessage(hwnd, WM_COPYDATA, (WPARAM) hwnd, (LPARAM) &cds);
			return 0;
		}
		else
			playlist_add(fnptr);
	}

	// Initialize our connection to the registry
	RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\Tom Grandgent", 0, "", 
		REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, (unsigned long *) &disposition);
	RegCloseKey(key);
	RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\Tom Grandgent\\TMIDI", 0, "", 
		REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, (unsigned long *) &disposition);
	
	// Read settings from the registry
	read_registry_settings();

	// Initialize the common controls
	InitCommonControls();

	// Check file-type associations
	check_associations();

	// Get the temp directory
	GetTempPath(sizeof(temp_dir), temp_dir);
	strcpy(analysis_file, temp_dir);
	strcat(analysis_file, "tmidi_analysis.txt");

	// Set process priority
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	// Determine performance counter frequency
	QueryPerformanceFrequency(&LIfreq);
	freq = LIfreq.LowPart / 1000;

	// Initialize GDI resources
	init_gdi_resources();

	// Show the main program dialog
	DialogBox(ghInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, (DLGPROC) MainDlg);

	// If the MIDI-in interface is still open, close it
	if (hin)
		close_midi_in();
	// If the MIDI-out interface is still open, close it
	if (hout)
		close_midi_out();

	// Write settings to the registry
	write_registry_settings();

	// Kill the analysis file
	unlink(analysis_file);

	// Free GDI resources
	free_gdi_resources();

	return 0;
}

// Main program dialog callback
INT_PTR CALLBACK MainDlg(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	static char buf[MAX_PATH + 64];
	char *ch;
	char filename[256] = "";
	OPENFILENAME ofn;
	int i, j, channel, category, instrument, ret;
	HWND hwndcb;
	POINT pt;
	HMENU hmenu, cmenu[16];
	MENUITEMINFO mii;
	HANDLE hDrop = NULL;
	static HBRUSH hGrayBrush = (HBRUSH) GetStockObject(LTGRAY_BRUSH);
	static int width, height;
	int numFiles = 0;
	HDC hdc;
	PAINTSTRUCT ps;
	RECT tmpRect;
	PCOPYDATASTRUCT pcds;
	LPNMHDR nmhdr;
//	HBRUSH brush;

	if (!hwndApp)
		hwndApp = hDlg;

	switch (iMsg)
	{
		case WM_INITDIALOG:
			// Set the main dialog's icon
			SetClassLongPtr(hDlg, GCLP_HICON,
				(LONG_PTR) LoadImage(ghInstance, MAKEINTRESOURCE(IDI_ICON), IMAGE_ICON,
						 GetSystemMetrics(SM_CXSMICON),
						 GetSystemMetrics(SM_CYSMICON), 0));

			// Create the status bar
			hwndStatusBar = CreateStatusWindow(WS_CHILD | WS_VISIBLE, "", hDlg, NULL);

			// Set up button icons
			SendMessage(GetDlgItem(hDlg, IDC_PLAY), BM_SETIMAGE, IMAGE_ICON, 
						(LPARAM) LoadImage(ghInstance, MAKEINTRESOURCE(IDI_PLAY), IMAGE_ICON, 
						GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
			SendMessage(GetDlgItem(hDlg, IDC_PAUSE), BM_SETIMAGE, IMAGE_ICON, 
						(LPARAM) LoadImage(ghInstance, MAKEINTRESOURCE(IDI_PAUSE), IMAGE_ICON, 
						GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
			SendMessage(GetDlgItem(hDlg, IDC_STOP), BM_SETIMAGE, IMAGE_ICON, 
						(LPARAM) LoadImage(ghInstance, MAKEINTRESOURCE(IDI_STOP), IMAGE_ICON, 
						GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
			enum_devices(hDlg);
			//init_midi_out(GetDlgItem(hDlg, IDC_MIDI_OUT)); // moved to the playback thread
			init_midi_in(GetDlgItem(hDlg, IDC_MIDI_IN));
			// Restore the window position
			if (appRectSaved)
			{
				RECT desk;
				GetWindowRect(GetDesktopWindow(), &desk);
				if (appRect.right < desk.left)
					appRect.left = desk.left;
				if (appRect.left > desk.right)
					appRect.left = desk.left;
				if (appRect.bottom < desk.top)
					appRect.top = desk.top;
				if (appRect.top > desk.bottom)
					appRect.top = desk.top;
				SetWindowPos(hDlg, 0, appRect.left, appRect.top, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
			}
			// Create the tooltip control
			//DoCreateDialogTooltip();
			// Set the playback head to the beginning of the playlist
			if (playlist)
			{
				playback_head = playlist;
				PostMessage(hDlg, WMAPP_LOADFILE, 0, 0);
			}
			// Initialize all channels to unmuted
			for (i = 0; i < 16; i++)
				set_channel_mute(i, FALSE);
			// Initialize MT-32 state values
			init_mt32_state();
			// Subclass the channel number button windows
			for (i = 0; i < 16; i++)
			{
				OldButtonProc = (WNDPROC) SetWindowLongPtr(GetDlgItem(hDlg, IDC_C0 + i), 
					GWLP_WNDPROC, (LONG_PTR) NewButtonProc);
				/*if (!OldButtonProc)
				{
					i = GetLastError();
					sprintf(buf, "Error %d trying to subclass button wndproc!");
					MessageBox(hwndApp, buf, "ARGH", MB_ICONERROR);
					break;
				}*/
			}

			// Fill the controller combo box
			hwndcb = GetDlgItem(hDlg, IDC_CONTROLLERS);
			SendMessage(hwndcb, CB_RESETCONTENT, 0, 0);
			SendMessage(hwndcb, CB_ADDSTRING, 0, (LPARAM) "[Auto]");
			SendMessage(hwndcb, CB_SETITEMDATA, 0, -1);
			j = 1;
			for (i = 0; i < sizeof(controller_names) / sizeof(char *); i++)
			{
				if (strcmp(controller_names[i], "Unknown controller"))
				{
					SendMessage(hwndcb, CB_ADDSTRING, 0, (LPARAM) controller_names[i]);
					SendMessage(hwndcb, CB_SETITEMDATA, j, i);		// Store the index of the controller
					j++;
				}
			}
			SendMessage(hwndcb, CB_SETCURSEL, 0, 0);

			return TRUE;

		case WM_LBUTTONUP:
		case WM_LBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MOUSEMOVE:
			handle_mousedown(LOWORD(lParam), HIWORD(lParam), wParam, iMsg);
			return TRUE;

		case WM_COPYDATA:
			pcds = (PCOPYDATASTRUCT) lParam;
			if (pcds->dwData == IPC_PLAY)
			{
				// We're being told by another instance that the user has executed 
				// an associated file, so handle it.

				// Clear the playlist, add this file, make it the current file, and load it up
				playlist_clear();
				playlist_add((char *) pcds->lpData);
				playback_head = playlist;
				//SetForegroundWindow(hDlg);
				PostMessage(hDlg, WMAPP_LOADFILE, 0, 0);
				return TRUE;
			}
			return FALSE;

		case WM_PAINT:
			// Paint the bars
			hdc = BeginPaint(hDlg, &ps);
			//SelectObject(hdc, hNoteBackgroundBrush);
			// Invalidate all of the channel bars
			for (i = 0; i < 16; i++)
				ms.channels[i].drawn = 1;
			// Update the whole channel display
			update_display(hdc);
			//for (i = 0; i < 16; i++)
			//	Rectangle(hdc, BAR_X, BAR_Y + i * BAR_VSPACE, BAR_X + BAR_WIDTH, BAR_Y + i * BAR_VSPACE + BAR_HEIGHT);
			EndPaint(hDlg, &ps);
			break;

		case WM_DESTROY:
			// Remove subclasses for the channel number button windows
			for (i = 0; i < 16; i++)
				SetWindowLongPtr(GetDlgItem(hDlg, IDC_C0 + i), 
					GWLP_WNDPROC, (LONG_PTR) OldButtonProc);
			// Remove the tooltip hook
			CleanupTooltip();
			break;

		case WM_CLOSE:
			GetWindowRect(hDlg, &tmpRect);
			if (tmpRect.left != -32000)
			{
				memcpy(&appRect, &tmpRect, sizeof(RECT));
				appRectSaved = 1;
			}
			close_midi_in();
			close_midi_out();
			EndDialog(hDlg, 0);
			break;

		case WMAPP_DONE_PLAYING:
			sprintf(buf, "Stopped playing %s.", extract_filename(ms.filename));
			SetWindowText(hwndStatusBar, buf);
			if (playback_head)
			{
				if (!strcmp(playback_head->filename, ms.filename))
				{
					if (ms.finished_naturally)
					{
						playback_head = playback_head->next;
						if (playback_head)
							PostMessage(hDlg, WMAPP_LOADFILE, 0, 0);
					}
				}
				else
					PostMessage(hDlg, WMAPP_LOADFILE, 0, 0);
			}
			break;

		case WMAPP_LOADFILE:
			if (ms.playing)
			{
				ms.stop_requested = 1;
				break;
			}
			if (playback_head)
			{
				if (!load_midi(playback_head->filename, hDlg))
					PostMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDC_PLAY, BN_CLICKED), (LPARAM) GetDlgItem(hDlg, IDC_PLAY));
				else
				{
					playback_head = playback_head->next;
					PostMessage(hDlg, WMAPP_LOADFILE, 0, 0);
				}
			}
			break;

		case WM_DROPFILES:
			hDrop = (HANDLE) wParam;
			numFiles = DragQueryFile((HDROP) hDrop, 0xFFFFFFFF, NULL, 0);
			playlist_clear();
			for (i = 0; i < numFiles; i++)
			{
				DragQueryFile((HDROP) hDrop, i, filename, sizeof(filename));
				playlist_add(filename);
			}
			playback_head = playlist;
			// Load the file that was dragged onto the window
			PostMessage(hDlg, WMAPP_LOADFILE, 0, 0);
			break;

		case WM_HSCROLL:
			if (((HWND) lParam) == GetDlgItem(hDlg, IDC_TEMPO_SLIDER))
			{
				if (!get_scroll_value(wParam, lParam, &i) && i > 0)
					set_tempo((int) (60000000.0f / i));
			}
			else
			if (((HWND) lParam) == GetDlgItem(hDlg, IDC_PITCH_SLIDER))
			{
				if (!get_scroll_value(wParam, lParam, &i))
					set_mod_pitch(i - 24);
			}
			else
			if (((HWND) lParam) == GetDlgItem(hDlg, IDC_VELOCITY_SLIDER))
			{
				if (!get_scroll_value(wParam, lParam, &i))
					set_mod_velocity(i - 64);
			}
			else
			if (((HWND) lParam) == GetDlgItem(hDlg, IDC_SONG_SLIDER))
			{
				i = 0;
				switch (LOWORD(wParam))
				{
					case TB_ENDTRACK:
						i = SendMessage((HWND) lParam, TBM_GETPOS, 0, 0);
						ms.seek_sliding = 0;
						if (!ms.seeking)
						{
							ms.seeking = 1;
							ms.seek_to = (double) (i * 1000);
						}
						break;
					case TB_THUMBTRACK:
					case TB_THUMBPOSITION:
						i = HIWORD(wParam);
						sprintf(buf, "%d:%02d / %d:%02d", i / 60, i % 60, ms.song_length / 60, ms.song_length % 60);
						SetDlgItemText(hDlg, IDC_SONG_LENGTH, buf);
						ms.seek_sliding = 1;
						break;
				}
			}
			break;

		case WM_NOTIFY:
			nmhdr = (LPNMHDR) lParam;
			if (nmhdr->hwndFrom == g_hwndTT && nmhdr->code == TTN_NEEDTEXT)
				OnWMNotify(lParam);
			break;

		case WM_COMMAND:
			// Look for instrument names getting clicked on
			if (LOWORD(wParam) >= IDC_T0 && LOWORD(wParam) <= IDC_T15)
			{
				// Create a popup menu
				GetCursorPos(&pt);
				hmenu = CreatePopupMenu();
				// Insert a default item
				sprintf(buf, "Default");
				memset(&mii, 0, sizeof(mii));
				mii.cbSize = sizeof(MENUITEMINFO);
				mii.fMask = MIIM_STATE | MIIM_ID | MIIM_TYPE;
				mii.fState = MFS_DEFAULT;
				mii.fType = MFT_STRING;
				mii.dwTypeData = buf;
				mii.cch = strlen(buf);
				mii.wID = 1;
				InsertMenuItem(hmenu, 0, TRUE, &mii);
				// Insert separator
				mii.fMask = MIIM_STATE | MIIM_TYPE;
				mii.fState = MFS_ENABLED;
				mii.fType = MFT_SEPARATOR;
				InsertMenuItem(hmenu, 1, TRUE, &mii);
				// Prepare for additional menu entries
				mii.fMask = MIIM_STATE | MIIM_ID | MIIM_TYPE;
				mii.fType = MFT_STRING;

				if (LOWORD(wParam) == IDC_T9)	// Percussion channel
				{
					if (HIWORD(wParam) == STN_CLICKED)
					{
						// Insert drum kit names
						for (i = 0; i < sizeof(drum_kit_names) / sizeof(char *); i++)
						{
							ch = strchr(drum_kit_names[i], '-');
							mii.dwTypeData = ch + 2;
							mii.cch = strlen(ch + 2);
							mii.wID = i + 2;
							InsertMenuItem(hmenu, i + 2, TRUE, &mii);
						}
					}
				}
				else	// Regular instrument channels
				{
					for (category = 0; category < 16; category++)
					{
						// Create a submenu for this instrument category
						cmenu[category] = CreatePopupMenu();
						for (instrument = 0; instrument < 8; instrument++)
						{
							mii.fMask = MIIM_ID | MIIM_TYPE;
							mii.wID = category * 8 + instrument + 2;
							mii.fType = MFT_STRING;
							mii.dwTypeData = get_program_name(mii.wID - 2, 0);
							InsertMenuItem(cmenu[category], 999, TRUE, &mii);
						}
						// Then create a submenu entry
						mii.fMask = MIIM_SUBMENU | MIIM_TYPE;
						mii.fType = MFT_STRING;
						mii.dwTypeData = program_categories[category];
						mii.hSubMenu = cmenu[category];
						InsertMenuItem(hmenu, 999, TRUE, &mii);
					}
				}
				// Display the menu
				SetForegroundWindow(hwndApp);
				ret = TrackPopupMenu(hmenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndApp, NULL);
				DestroyMenu(hmenu);
				if (!ret)
					break;
				// If they chose default, unset a program override
				if (ret == 1)
					set_program_override(LOWORD(wParam) - IDC_T0, FALSE, 0);
				else	// Otherwise, set a program override
				{
					// Figure out what program # to use
					if (LOWORD(wParam) == IDC_T9)
						i = atoi(drum_kit_names[ret - 2]);
					else
						i = ret - 2;
					set_program_override(LOWORD(wParam) - IDC_T0, TRUE, i);
				}
			}
			else
			if (LOWORD(wParam) >= IDC_C0 && LOWORD(wParam) <= IDC_C15)
			{
				channel = LOWORD(wParam) - IDC_C0;
				switch (HIWORD(wParam))
				{
					case BN_CLICKED:
						set_channel_mute(channel, !ms.channels[channel].muted);
						SetFocus(GetDlgItem(hDlg, IDC_STOP));
						break;
					case WM_RBUTTONUP:	// WHO'S THE MAN!!!!!!!!!
					//case BN_DBLCLK:
						set_channel_solo(channel);
						SetFocus(GetDlgItem(hDlg, IDC_STOP));
						break;
				}
			}
			else	// Handle regular control events
			switch (LOWORD(wParam))
			{
				case IDC_ALL_INSTR:
					if (HIWORD(wParam) == BN_CLICKED)
					{
						if (ms.channels[0].program_overridden)
						{
							for (i = 1; i < 16; i++)
								if (i != 9)
									set_program_override(i, TRUE, ms.channels[0].override_program);
						}
						else
						{
							for (i = 0; i < 16; i++)
								if (ms.channels[i].program_overridden)
									break;
							if (i != 16)
								for (i = 0; i < 16; i++)
									set_program_override(i, FALSE, 0);
							else
								MessageBox(hwndApp, "To use the All Instr. button, override the instrument of the first channel by clicking on its instrument name and choosing another channel.  Then hit the All Instr. button to override all channels with that instrument.  To turn off the overrides, set the first channel back to its default instrument and hit the All Instr. button again.", "All Instr. Explanation", MB_ICONINFORMATION);
						}
					}
					break;
				case IDC_PITCH:
					set_mod_pitch(0);
					break;
				case IDC_VELOCITY:
					ms.mod_velocity = 0;
					SendDlgItemMessage(hDlg, IDC_VELOCITY_SLIDER, TBM_SETPOS, TRUE, 64);
					SetDlgItemText(hDlg, IDC_VELOCITY, "Velocity: 0");
					break;
				case IDC_MIDI_OUT:
					//if (HIWORD(wParam) == CBN_SELCHANGE)
					//	init_midi_out((HWND) lParam);
					break;
				case IDC_MIDI_IN:
					if (HIWORD(wParam) == CBN_SELCHANGE)
						init_midi_in((HWND) lParam);
					break;
				case IDC_CONTROLLERS:
					if (HIWORD(wParam) == CBN_SELCHANGE)
					{
						// Get the selection index
						hwndcb = (HWND) lParam;
						i = SendMessage(hwndcb, CB_GETCURSEL, 0, 0);
						// Get the controller ID
						j = SendMessage(hwndcb, CB_GETITEMDATA, (WPARAM) i, 0);
						// Check auto vs. other
						if (j == -1)
						{
							for (i = 0; i < 16; i++)
							{
								ms.channels[i].lock_controller = 0;
								ms.channels[i].displayed_controller = ms.channels[i].last_controller;
								ms.channels[i].drawn = 1;
							}
							update_display(NULL);
						}
						else
						{
							for (i = 0; i < 16; i++)
							{
								ms.channels[i].lock_controller = 1;
								ms.channels[i].displayed_controller = j;
								ms.channels[i].drawn = 1;
							}
							update_display(NULL);
						}
					}
					break;
				case IDC_PLAY:
					if (HIWORD(wParam) == BN_CLICKED)
						if (!(ms.playing) && ms.filename && ms.filename[0])
							_beginthread(playback_thread, 0, NULL);
						else
							if (ms.paused)
								ms.paused = 0;
					break;
				case IDC_STOP:
					if (HIWORD(wParam) == BN_CLICKED)
						ms.stop_requested = 1;
					break;
				case IDC_PAUSE:
					if (HIWORD(wParam) == BN_CLICKED)
						ms.paused = !(ms.paused);
					break;
				case IDM_FILE_OPEN:
				case IDC_OPEN:
					if (HIWORD(wParam) == BN_CLICKED)
					{
						if (ms.playing)
							ms.stop_requested = 1;
						// Get the filename from the user with the common open file dialog
						ZeroMemory(&ofn, sizeof(OPENFILENAME));
						ofn.lStructSize = sizeof(OPENFILENAME);
						ofn.hwndOwner = hDlg;
						ofn.lpstrFile = filename;
						ofn.nMaxFile = sizeof(filename);
						ofn.lpstrFilter = "MIDI Files (*.mid;*.rmi)\0*.mid;*.rmi\0All Files (*.*)\0*.*\0";
						ofn.nFilterIndex = 0;
						ofn.lpstrFileTitle = NULL;
						ofn.nMaxFileTitle = 0;
						ofn.lpstrInitialDir = NULL;
						ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
						i = GetOpenFileName(&ofn);
						if (i)
							load_midi(filename, hDlg);
						/*else
						{
							i = CommDlgExtendedError();
							itoa(i, filename, 10);
							MessageBox(hwndApp, "argh!", filename, MB_ICONERROR);
						}*/
					}
					break;
				case IDC_ANALYSIS:
					if (HIWORD(wParam) == BN_CLICKED)
					{
						ShellExecute(hDlg, "open", "notepad", analysis_file, NULL, SW_SHOWNORMAL);
					}
					break;
				case IDC_CURRENT_TEXT:
				case IDC_DISPLAY_TEXT:
					if (HIWORD(wParam) == BN_CLICKED || HIWORD(wParam) == STN_CLICKED)
					{
						if (hwndText)
							PostMessage(hwndText, WM_CLOSE, 0, 0);
							//SetForegroundWindow(hwndText);
						else
							hwndText = CreateDialog(ghInstance, MAKEINTRESOURCE(IDD_TEXT), hwndApp, (DLGPROC) TextDlg);
					}
					break;
				case IDC_DISPLAY_TRACKS:
					if (HIWORD(wParam) == BN_CLICKED)
					{
						if (hwndTracks)
							PostMessage(hwndTracks, WM_CLOSE, 0, 0);
							//SetForegroundWindow(hwndText);
						else
							hwndTracks = CreateDialog(ghInstance, MAKEINTRESOURCE(IDD_TRACKS), hwndApp, (DLGPROC) TracksDlg);
					}
					break;
				case IDC_DISPLAY_CHANNELS:
					if (HIWORD(wParam) == BN_CLICKED)
					{
						if (hwndChannels)
							PostMessage(hwndChannels, WM_CLOSE, 0, 0);
							//SetForegroundWindow(hwndText);
						else
							hwndChannels = CreateDialog(ghInstance, MAKEINTRESOURCE(IDD_CHANNELS), hwndApp, (DLGPROC) ChannelsDlg);
					}
					break;
				case IDC_DISPLAY_SYSEX:
					if (HIWORD(wParam) == BN_CLICKED)
					{
						if (hwndSysex)
							PostMessage(hwndSysex, WM_CLOSE, 0, 0);
							//SetForegroundWindow(hwndText);
						else
							hwndSysex = CreateDialog(ghInstance, MAKEINTRESOURCE(IDD_SYSEX), hwndApp, (DLGPROC) SysexDlg);
					}
					break;
				case IDM_OUTCONFIG:
					DialogBox(ghInstance, MAKEINTRESOURCE(IDD_OUTCONFIG), hDlg, (DLGPROC) OutConfigDlg);
					break;
				case IDOK:
					return TRUE;
				case IDM_FILE_EXIT:
				case IDCANCEL:
					EndDialog(hDlg, 0);
					return TRUE;
			}
	}

	return FALSE;
}

void enum_devices(HWND hwnd)
{
	int indevs, outdevs;
	MIDIINCAPS incaps;
	MIDIOUTCAPS outcaps;
	int i, ret;
	HWND hwndcb;
	midi_device_t *dev, *ld = NULL;

	hwndcb = GetDlgItem(hwnd, IDC_MIDI_IN);
	SendMessage(hwndcb, CB_RESETCONTENT, 0, 0);
	SendMessage(hwndcb, CB_ADDSTRING, 0, (LPARAM) "[None]");
	indevs = midiInGetNumDevs();
	for (i = 0; i < indevs; i++)
	{
		ret = midiInGetDevCaps(i, &incaps, sizeof(incaps));
		if (ret != MMSYSERR_NOERROR)
			continue;

		// Set up a device node for this device
		dev = (midi_device_t *) calloc(1, sizeof(midi_device_t));
		dev->user_device_name = strdup(incaps.szPname);
		dev->input_device = 1;
		memcpy(&dev->incaps, &incaps, sizeof(MIDIINCAPS));
		dev->usable = 1;
		dev->standards = MIDI_STANDARD_GM;
		dev->next = NULL;
		if (!ld)
			midi_devices = dev;
		else
			ld->next = dev;
		ld = dev;
		
		SendMessage(hwndcb, CB_ADDSTRING, 0, (LPARAM) incaps.szPname);
	}
	SendMessage(hwndcb, CB_SETCURSEL, (WPARAM) midi_in_cb, 0);

	hwndcb = GetDlgItem(hwnd, IDC_MIDI_OUT);
	SendMessage(hwndcb, CB_RESETCONTENT, 0, 0);
	SendMessage(hwndcb, CB_ADDSTRING, 0, (LPARAM) "[None]");
	outdevs = midiOutGetNumDevs();
	for (i = 0; i < outdevs; i++)
	{
		ret = midiOutGetDevCaps(i, &outcaps, sizeof(outcaps));
		if (ret != MMSYSERR_NOERROR)
			continue;

		// Set up a device node for this device
		dev = (midi_device_t *) calloc(1, sizeof(midi_device_t));
		dev->user_device_name = strdup(outcaps.szPname);
		memcpy(&dev->outcaps, &outcaps, sizeof(MIDIOUTCAPS));
		dev->usable = 1;
		dev->standards = MIDI_STANDARD_GM;
		dev->next = NULL;
		if (!ld)
			midi_devices = dev;
		else
			ld->next = dev;
		ld = dev;

		SendMessage(hwndcb, CB_ADDSTRING, 0, (LPARAM) outcaps.szPname);
	}
	SendMessage(hwndcb, CB_SETCURSEL, (WPARAM) midi_out_cb, 0);
}

void init_midi_in(HWND hwndcb)
{
	int device;
	char msgbuf[256];

	close_midi_in();

	midi_in_cb = device = SendMessage(hwndcb, CB_GETCURSEL, 0, 0);
	if (!device)
		return;
	device--;

	if (midiInOpen(&hin, device, (DWORD) MidiInProc, 0, CALLBACK_FUNCTION) != MMSYSERR_NOERROR)
	{
		hin = NULL;
		strcpy(msgbuf, "Unable to open MIDI-in device:\n");
		SendMessage(hwndcb, CB_GETLBTEXT, (WPARAM) device + 1, (LPARAM) (LPCSTR) &msgbuf[strlen(msgbuf)]);
		MessageBox(hwndApp, msgbuf, "midiInOpen() failed...", MB_ICONERROR);
	}
	if (hin)
	{
		midiInStart(hin);
		init_midi_out(GetDlgItem(hwndApp, IDC_MIDI_OUT));
	}
}

void init_midi_out(HWND hwndcb)
{
	int device;
	char msgbuf[256];

	if (hout)
	{
		midiOutClose(hout);
		hout = NULL;
	}

	midi_out_cb = device = SendMessage(hwndcb, CB_GETCURSEL, 0, 0);
	if (!device)
		return;
	device--;

	if (midiOutOpen(&hout, device, NULL, 0, NULL) != MMSYSERR_NOERROR)
	{
		hout = NULL;
		strcpy(msgbuf, "Unable to open MIDI-out device:\n");
		SendMessage(hwndcb, CB_GETLBTEXT, (WPARAM) device + 1, (LPARAM) (LPCSTR) &msgbuf[strlen(msgbuf)]);
		MessageBox(hwndApp, msgbuf, "midiOutOpen() failed...", MB_ICONERROR);
	}
}

void close_midi_in(void)
{
	if (hin)
	{
		midiInStop(hin);
		midiInClose(hin);
		hin = NULL;
	}
}

void close_midi_out(void)
{
	int i = 0;

	if (hin)
		return;

	if (hout)
	{
		all_notes_off();
		midiOutReset(hout);
		while (midiOutClose(hout) != MMSYSERR_NOERROR && i++ < 10)
			Sleep(200);
		if (i == 10)
		{
			MessageBox(hwndApp, "Unable to close MIDI-out device!", "TMIDI Error", MB_ICONERROR);
		}
		hout = NULL;
	}
}

void note_on(unsigned char on, unsigned char note, unsigned char velocity, unsigned char chan)
{
	DWORD dwParam1;
	signed int i;
	unsigned char channel = chan;

	// Perform global velocity modulation
	if (velocity && ms.mod_velocity)
	{
		i = (signed int) velocity;
		i += ms.mod_velocity;
		if (i < 0)
			i = 0;
		if (i > 127)
			i = 127;
		velocity = (unsigned char) i;
	}
	// Perform global pitch modulation
	if (channel != 9 && ms.channels[channel].last_bank != 127)
		note += ms.mod_pitch;

	// Update display/historic values
	update_note_volume(channel, note, on ? velocity : 0);
	ms.channels[channel].last_note_pitch = note;
	ms.channels[channel].last_note_velocity = velocity;

	// Don't actually play the note if this channel is muted
	if (ms.channels[channel].muted)
		return;

	// If the bank for this channel is 127, treat it as a percussive channel! (XG)
	// This needs to be an option, in case someone actually has a real XG synth.
	if (ms.midi_standard == MIDI_STANDARD_XG && ms.channels[channel].last_bank == 127)
		channel = 9;

	dwParam1 = MAKELONG(MAKEWORD(MAKEBYTE(channel, on ? 9 : 8), note), MAKEWORD(velocity, 0));
	if (hout)
		midiOutShortMsg(hout, dwParam1);
}

// Turns all notes off on a specific channel
void all_notes_off_channel(int channel)
{
	// Clear saved note velocities for this channel
	memset(&ms.channels[channel].notes, 0, 128);
	ms.channels[channel].note_count = 0;
	// Turn off notes on the MIDI-out device
	if (hout)
	{
		// Better safe than sorry!
		midiOutShortMsg(hout, MAKELONG(MAKEWORD(0xB0 + channel, 120), MAKEWORD(0, 0)));
		midiOutShortMsg(hout, MAKELONG(MAKEWORD(0xB0 + channel, 121), MAKEWORD(0, 0)));
		midiOutShortMsg(hout, MAKELONG(MAKEWORD(0xB0 + channel, 123), MAKEWORD(0, 0)));
	}
}

// Turns all notes off on all channels
void all_notes_off(void)
{
	int c;

	for (c = 0; c < 16; c++)
		all_notes_off_channel(c);
}

void CALLBACK MidiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	static char buf[256];
	unsigned char status, byte1, byte2;
	unsigned char event, channel;

	switch (wMsg)
	{
		case MIM_OPEN:
//			printf("MIDI input opened\n");
			break;
		case MIM_CLOSE:
//			printf("MIDI input closed\n");
			break;
		case MIM_DATA:
			//printf("Data: %d  Timestamp: %d\n", dwParam1, dwParam2);
			byte1 = HIBYTE(LOWORD(dwParam1));
			byte2 = LOBYTE(HIWORD(dwParam1));
			status = LOBYTE(LOWORD(dwParam1));
			event = HINYBBLE(status);
			channel = LONYBBLE(status);
			switch (event)
			{
				case 9:		// Note on
					//printf("Note %s - note %d, velocity %d\n", byte2 ? "on" : "off", byte1, byte2);
					note_on(TRUE, byte1, byte2, channel);
					break;
				case 0x0B:	// Controller change
					ms.channels[channel].last_controller = byte1;
					ms.channels[channel].last_controller_value = byte2;
					if (byte1 == 0)
						ms.channels[channel].last_bank = byte2;
					midiOutShortMsg(hout, dwParam1);
					break;
				case 0x0C:	// Program change
					ms.channels[channel].last_program = byte1;
					midiOutShortMsg(hout, dwParam1);
					break;
				default:
					//OutputDebugString("Sending data\n");
					midiOutShortMsg(hout, dwParam1);
			}
			if (hwndChannels)
				PostMessage(hwndChannels, WMAPP_REFRESH_CHANNELS, 0, 0);
//			printf("Channel: %d  Event: %d  Byte1: %d  Byte2: %d\n", channel, event, byte1, byte2);
//			send_midi_data(dwParam1);
			break;
		default:
//			printf("Unknown MIDI IN message: %d - %d - %d\n", wMsg, dwParam1, dwParam2);
			if (hout)
				midiOutShortMsg(hout, dwParam1);
//			send_midi_data(dwParam1);
	}
}

int load_midi(char *filename, HWND hDlg)
{
	FILE *infile = NULL, *outfile = NULL;
	char *MThd = "MThd";
	char *MTrk = "MTrk";
	char *RIFF = "RIFF";
	//unsigned char *text;
	unsigned char id[4];
	char buf[1024];
	unsigned int track = 0, i, len, endtrack = 0;
	//unsigned int offset, delta, intd;
	//char eventname[128];
	//unsigned char lastcmd = 0, cmd, d1, d2, d3, d4, d5;
	//signed char sd1;
	int success = 0;

	// Save the given filename
	strcpy(ms.filename, filename);

	// Dump any previously-loaded MIDI text and sysex events
	kill_all_midi_text();
	kill_all_midi_sysex();

	// Unload any previously-loaded MIDI data
	if (mh.num_tracks)
	{
		for (i = 0; i < mh.num_tracks; i++)
		{
			th[i].dataptr = NULL;
			if (th[i].data)
				free(th[i].data);
			memset(&th[i], 0, sizeof(track_header_t));
		}
		free(th);
		th = NULL;
	}

	// Reset percussion flag
	ms.uses_percussion = 0;

	// Reset highest pitch bend value to zero
	ms.highest_pitch_bend = 0;

	// Reset channel states
	for (i = 0; i < 16; i++)
	{
		ms.channels[i].program_overridden = 0;
		ms.channels[i].muted = 0;
	}

	// Decide whether or not to perform pre-analysis
	ms.perform_analysis = 0;

	if (ms.perform_analysis)
	{
		outfile = fopen(analysis_file, "w");
		if (!outfile)
		{
			MessageBox(hwndApp, "Unable to open file for writing.", "analysis.txt", MB_ICONERROR);
			return 1;
		}
	}
	infile = fopen(filename, "rb");
	if (!infile)
	{
		MessageBox(hwndApp, "Unable to open file for reading.", filename, MB_ICONERROR);
		return 1;
	}

	if (strlen(filename) > 4 && stristr(&filename[strlen(filename) - 4], ".SYX"))
	{
		handle_sysex_dump(infile);
		// Disable the "Display Analysis" button
		EnableWindow(GetDlgItem(hwndApp, IDC_ANALYSIS), FALSE);
		// Disable the "Display Text" button
		EnableWindow(GetDlgItem(hwndApp, IDC_DISPLAY_TEXT), FALSE);
		// Disable playback control button
		EnableWindow(GetDlgItem(hwndApp, IDC_PLAY), FALSE);

		return 1;
	}

	__try
	{
	if (ms.perform_analysis)
		fprintf(outfile, "Analysis of %s\n\n", filename);

	// Read the MIDI file header
	read_bytes(infile, id, 4);

	// Check to see if the "MThd" ID is correct
	if (memcmp(MThd, id, 4))
	{
		// No?  Check for a RIFF header (RMID format)
		if (!memcmp(RIFF, id, 4))
		{
			// Yes!  Ok, skip past 4 byte filesize + 4 byte RMID
			read_bytes(infile, id, 4);
			read_bytes(infile, id, 4);
			read_bytes(infile, id, 4);
			read_bytes(infile, id, 4);
			// Now read the MIDI file header
			read_bytes(infile, id, 4);
			// Now look for MThd
			if (memcmp(MThd, id, 4))
			{
				sprintf(buf, "'%s' is not a MIDI file because it does not begin with \"MThd\".", filename);
				if (ms.perform_analysis)
					fprintf(outfile, "%s\n", buf);
				MessageBox(hwndApp, buf, "TMIDI Error", MB_ICONERROR);
				return 1;
			}			
		}
		else
		{
			// STILL no MThd?  Search through the beginning of the file for MThd
			fseek(infile, 0, SEEK_SET);
			len = fread(buf, 1, sizeof(buf), infile);
			if (len > 4)
				for (i = 0; i < len - 4; i++)
					if (!memcmp(MThd, &buf[i], 4))
						break;
			if (len <= 4 || i == len - 4)
			{
				sprintf(buf, "'%s' is not a MIDI file because it does not begin with \"MThd\".", filename);
				if (ms.perform_analysis)
					fprintf(outfile, "%s\n", buf);
				MessageBox(hwndApp, buf, "TMIDI Error", MB_ICONERROR);
				return 1;
			}
			else
			{
				fseek(infile, i, SEEK_SET);
				read_bytes(infile, id, 4);
				if (ms.perform_analysis)
					fprintf(outfile, "Warning: File is damaged... %d bytes of unexpected data before MThd header.\n", i);
			}
		}
	}

	// Read the header size
	mh.header_size = read_int(infile);
	if (ms.perform_analysis)
		fprintf(outfile, "Header size: %d bytes\n", mh.header_size);

	// Identify the file format
	mh.file_format = read_short(infile);
	if (ms.perform_analysis)
	{
		fprintf(outfile, "File format: ");
		switch (mh.file_format)
		{
			case 0: fprintf(outfile, "Type 0: Single-track\n"); break;
			case 1: fprintf(outfile, "Type 1: Multiple tracks, synchronous\n"); break;
			case 2: fprintf(outfile, "Type 2: Multiple tracks, asynchronous\n"); break;
			default: fprintf(outfile, "Unknown (%d)", mh.file_format);
				sprintf(buf, "Unknown MIDI file type: %d", mh.file_format);
				MessageBox(hwndApp, buf, "TMIDI Error", MB_ICONERROR);
				return 1;
		}
	}

	// Output other information present in the header
	mh.num_tracks = read_short(infile);
	mh.num_ticks = read_short(infile);
	if (ms.perform_analysis)
	{
		fprintf(outfile, "Number of tracks: %d\n", mh.num_tracks);
		fprintf(outfile, "Ticks per quarter note: %d\n", mh.num_ticks);
	}

	// Allocate track headers
	th = (track_header_t *)calloc(mh.num_tracks, sizeof(track_header_t));

	// Read tracks
	while (!feof(infile))
	{
		// Read the track header
		if (!read_bytes(infile, id, 4))
			continue;
		// Check to see if the "MTrk" ID is correct
		if (memcmp(MTrk, id, 4))
		{
			if (ms.perform_analysis)
				fprintf(outfile, "Track header %d is not correct because it does not begin with \"MTrk\".\n", track + 1);
			if (track < mh.num_tracks)
			{
				sprintf(buf, "Track header %d is not correct because it does not begin with \"MTrk\"", track + 1);
				MessageBox(hwndApp, buf, "TMIDI Warning", MB_ICONWARNING);
			}
			break;
		}
		// Read track length in bytes
		th[track].length = read_int(infile);
		if (ms.perform_analysis)
			fprintf(outfile, "\n---- Track %d (%d bytes) ----\n\n", track + 1, th[track].length);

		// Read the track data into memory
		th[track].data = (unsigned char *) malloc(th[track].length);
		if (!th[track].data)
		{
			MessageBox(hwndApp, "malloc() failed trying to allocate memory for track data", "TMIDI Error", MB_ICONERROR);
			break;
		}
		fread(th[track].data, th[track].length, 1, infile);
/*		fseek(infile, -1 * (signed int) th[track].length, SEEK_CUR);

		// Read MIDI events
		offset = 0;
		endtrack = 0;
		while (!feof(infile) && !endtrack)
		{
			// Read a delta time
			delta = read_vlq(infile, &offset);

			if (ms.perform_analysis)
				fprintf(outfile, "DT: %4d - ", delta);

			// Read the MIDI event command
			cmd = fgetc(infile);
			offset++;

			// Handle running mode
			if (cmd < 128)
			{
				ungetc(cmd, infile);
				offset--;
				cmd = lastcmd;
			}
			else
				lastcmd = cmd;

			if (cmd == 0xFF)		// Meta-event
			{
				cmd = fgetc(infile);
				offset++;
				len = read_vlq(infile, &offset);
				text = NULL;
				switch (cmd)
				{
					case 0x00:		// Sequence number
						intd = read_short(infile);
						offset += 2;
						if (ms.perform_analysis)
							fprintf(outfile, "Set Sequence number: %d\n", intd);
						break;
					case 1:		// Text
					case 2:		// Copyright info
					case 3:		// Sequence or track name
					case 4:		// Track instrument name
					case 5:		// Lyric
					case 6:		// Marker
					case 7:		// Cue point
						strcpy(eventname, midi_text_event_descriptions[cmd]);
						text = (unsigned char *) malloc(len + 1);
						read_bytes(infile, text, len);
						offset += len;
						text[len] = '\0';
						if (ms.perform_analysis)
						{
							fprintf(outfile, "%s: ", eventname);
							fputs((const char *) text, outfile);
							fprintf(outfile, "\n");
						}
						break;
					case 0x20:		// MIDI Channel prefix
						d1 = fgetc(infile);
						offset++;
						if (ms.perform_analysis)
							fprintf(outfile, "MIDI Channel Prefix: %d\n", d1);
						break;
					case 0x21:		// MIDI Port
						d1 = fgetc(infile);
						offset++;
						if (ms.perform_analysis)
							fprintf(outfile, "MIDI Port: %d\n", d1);
						break;
					case 0x2F:		// End of track
						if (ms.perform_analysis)
							fprintf(outfile, "End of track\n");
						offset = th[track].length;
						endtrack = 1;
						continue;
					case 0x51:		// Set tempo
						read_bytes(infile, id, 3);
						offset += 3;
						intd = (id[0] << 16) + (id[1] << 8) + id[2];
						if (ms.perform_analysis)
							fprintf(outfile, "Set tempo: %d (%02X %02X %02X) - %.2fms\n", intd, id[0], id[1], id[2], 
								(double) (intd / 1000) / (double) mh.num_ticks);
						break;
					case 0x54:		// SMPTE Offset
						d1 = fgetc(infile);
						d2 = fgetc(infile);
						d3 = fgetc(infile);
						d4 = fgetc(infile);
						d5 = fgetc(infile);
						offset += 5;
						if (ms.perform_analysis)
							fprintf(outfile, "SMPTE Offset: %d:%02d:%02d.%02d.%02d\n", d1, d2, d3, d4, d5);
						break;
					case 0x58:		// Time signature
						d1 = fgetc(infile);
						d2 = fgetc(infile);
						d3 = fgetc(infile);
						d4 = fgetc(infile);
						offset += 4;
						if (ms.perform_analysis)
							fprintf(outfile, "Time signature: %d/%d, %d ticks in metronome click, %d 32nd notes to quarter note, %d quarter notes per measure\n", d1, d2, d3, d4, (4 * d1) / d2);
						break;
					case 0x59:		// Key signature
						sd1 = fgetc(infile);
						d2 = fgetc(infile);
						offset += 2;
						if (ms.perform_analysis)
						{
							fprintf(outfile, "Key signature: ");
							if (sd1 < 0)
								fprintf(outfile, "%d flats, ", 0 - sd1);
							else
								if (sd1 > 0)
									fprintf(outfile, "%d sharps, ", sd1);
								else
									fprintf(outfile, "Key of C, ");
							fprintf(outfile, d2 ? "minor\n" : "major\n");
						}
						break;
					case 0x7F:		// Sequencer-specific information
						if (ms.perform_analysis)
						{
							fprintf(outfile, "Sequencer-specific information, %d bytes: ", len);
							for (i = 0; i < len; i++)
								fputc(fgetc(infile), outfile);
							offset += len;
							fprintf(outfile, "\n");
						}
						else
						{
							for (i = 0; i < len; i++)
								fgetc(infile);
							offset += len;
						}
						break;
					default:		// Unknown
						if (ms.perform_analysis)
						{
							fprintf(outfile, "Meta-event, unknown command %X length %d: ", cmd, len);
							for (i = 0; i < len; i++)
								fputc(fgetc(infile), outfile);
							offset += len;
							fprintf(outfile, "\n");
						}
						else
						{
							for (i = 0; i < len; i++)
								fgetc(infile);
							offset += len;
						}
				}
				if (text)
				{
					free(text);
					text = NULL;
				}
			}
			else
			switch (HINYBBLE(cmd))	// Normal event
			{
				case 0x08: // Note off
					d1 = fgetc(infile);
					d2 = fgetc(infile);
					offset += 2;
					if (ms.perform_analysis)
						fprintf(outfile, "Note Off, note %d velocity %d\n", d1, d2);
					break;
				case 0x09: // Note on
					d1 = fgetc(infile);
					d2 = fgetc(infile);
					offset += 2;
					if (ms.perform_analysis)
						fprintf(outfile, "Note On, note %d velocity %d\n", d1, d2);
					if (LONYBBLE(cmd) == 9)
						ms.uses_percussion = 1;
					break;
				case 0x0A: // Key after-touch
					d1 = fgetc(infile);
					d2 = fgetc(infile);
					offset += 2;
					if (ms.perform_analysis)
						fprintf(outfile, "Key After-touch, note %d velocity %d\n", d1, d2);
					break;
				case 0x0B: // Control Change
					d1 = fgetc(infile);
					d2 = fgetc(infile);
					offset += 2;
					if (ms.perform_analysis)
						fprintf(outfile, "Control Change, controller %d value %d\n", d1, d2);
					break;
				case 0x0C: // Program Change
					d1 = fgetc(infile);
					offset++;
					if (ms.perform_analysis)
					{
						fprintf(outfile, "Program Change, program %d", d1);
						if (LONYBBLE(cmd) != 9)
							fprintf(outfile, " (%s)\n", get_program_name(d1));
						else
							fprintf(outfile, "\n");
					}
					break;
				case 0x0D: // Channel after-touch
					d1 = fgetc(infile);
					offset++;
					if (ms.perform_analysis)
						fprintf(outfile, "Channel After-touch, channel %d\n", d1);
					break;
				case 0x0E: // Pitch wheel
					intd = read_short(infile);
					offset += 2;
					if (ms.perform_analysis)
						fprintf(outfile, "Pitch Wheel, amount %d\n", intd);
					break;
				case 0x0F: // System message
					switch (LONYBBLE(cmd))
					{
						case 0x02:
							d1 = fgetc(infile);
							d2 = fgetc(infile);
							offset += 2;
							intd = (d2 << 8) + d1;
							if (ms.perform_analysis)
								fprintf(outfile, "System Message: Song position: %d\n", intd);
						case 0x03:
							d1 = fgetc(infile);
							offset++;
							if (ms.perform_analysis)
								fprintf(outfile, "System Message: Song select: %d\n", d1);
						case 0x06:
							if (ms.perform_analysis)
								fprintf(outfile, "System Message: Tune request\n");
							break;
						case 0x00:
						case 0x07:
							len = read_vlq(infile, &offset);
							if (ms.perform_analysis)
							{
								fprintf(outfile, "SysEx Data, length %d: %02X ", len, cmd);
								for (i = 0; i < len; i++)
									fprintf(outfile, "%02X ", fgetc(infile));
								fprintf(outfile, "\n");
							}
							else
								for (i = 0; i < len; i++)
									fgetc(infile);
							offset += len;
							break;
						case 0x08:
							if (ms.perform_analysis)
								fprintf(outfile, "System Message: Timing clock used when synchronization is required.\n");
							break;
						case 0x0A:
							if (ms.perform_analysis)
								fprintf(outfile, "System Message: Start current sequence\n");
							break;
						case 0x0B:
							if (ms.perform_analysis)
								fprintf(outfile, "System Message: Continue a stopped sequence where left off\n");
							break;
						case 0x0C:
							if (ms.perform_analysis)
								fprintf(outfile, "System Message: Stop a sequence\n");
							break;
						default:
							if (ms.perform_analysis)
								fprintf(outfile, "System Message: Unknown (%d)\n", LONYBBLE(cmd));
					}
					break;
				default:
					if (ms.perform_analysis)
						fprintf(outfile, "Unknown Message (%d)\n", HINYBBLE(cmd));
					fgetc(infile);
					offset++;
			}
		}*/
		track++;
		if (track >= MAX_MIDI_TRACKS)
		{
			sprintf(buf, "Warning: Max number of MIDI tracks reached (%d).\n\nI refuse to read any more from this file.", MAX_MIDI_TRACKS);
			MessageBox(hwndApp, buf, "TMIDI Warning", MB_ICONWARNING);
			break;
		}
	}
	success = 1;

	}
	__finally
	{
		if (mh.num_tracks != track)
		{
			//sprintf(buf, "Warning: MIDI header indicated %d tracks but %d were found in the file.", mh.num_tracks, track);
			//MessageBox(hwndApp, buf, "Warning", MB_ICONWARNING);
			mh.num_tracks = track;
		}
		if (infile)
			fclose(infile);
		if (outfile)
			fclose(outfile);
		if (success)
		{
			sprintf(buf, "Loaded %s, analyzing...", filename);
			SetWindowText(hwndStatusBar, buf);
			if (analyze_midi())
				goto LoadFailed;
			sprintf(buf, "Loaded %s.", filename);
			SetWindowText(hwndStatusBar, buf);
			strcpy(ms.filename, filename);
			// Enable the "Display Analysis" button
			EnableWindow(GetDlgItem(hwndApp, IDC_ANALYSIS), ms.perform_analysis);
			// Enable or disable the "Text" button depending on whether or not text was loaded
			EnableWindow(GetDlgItem(hwndApp, IDC_DISPLAY_TEXT), midi_text_events ? TRUE : FALSE);
			// Enable or disable the "Sysex" button depending on whether or not text was loaded
			EnableWindow(GetDlgItem(hwndApp, IDC_DISPLAY_SYSEX), midi_sysex_events ? TRUE : FALSE);
			// Enable playback control button
			EnableWindow(GetDlgItem(hwndApp, IDC_PLAY), TRUE);
		}
		else
		{
LoadFailed:
			sprintf(buf, "Failed to load %s.", extract_filename(filename));
			SetWindowText(hwndStatusBar, buf);
			// Disable the "Display Analysis" button
			EnableWindow(GetDlgItem(hwndApp, IDC_ANALYSIS), FALSE);
			// Disable the "Display Text" button
			EnableWindow(GetDlgItem(hwndApp, IDC_DISPLAY_TEXT), FALSE);
			// Disable playback control button
			EnableWindow(GetDlgItem(hwndApp, IDC_PLAY), FALSE);
		}
	}

	return 0;
}

void reverse_endian_word(unsigned short int *word)
{
	unsigned char high, low;

	high = HIBYTE(*word);
	low = LOBYTE(*word);
	*word = MAKEWORD(high, low);
}

int read_bytes(FILE *fp, unsigned char *buf, int num)
{
	return fread(buf, num, 1, fp);
}

unsigned int read_int(FILE *fp)
{
	unsigned short high, low;

	high = read_short(fp);
	low = read_short(fp);

	return ((high << 16) + low);
}

unsigned short int read_short(FILE *fp)
{
	unsigned char high, low;

	high = fgetc(fp);
	low = fgetc(fp);
	return MAKEWORD(low, high);
}

unsigned int read_vlq(FILE *fp, unsigned int *offset)
{
    unsigned int value;
    unsigned char c, i = 0;

    if ((value = getc(fp)) & 0x80)
    {
        value &= 0x7f;
        do
        {
			value = (value << 7) + ((c = getc(fp)) & 0x7f);
			(*offset)++;
        } while (c & 0x80 && c != 255); // && ++i < 3);
    }
	(*offset)++;

    return value;
}

int read_bytes_mem(track_header_t *th, unsigned char *buf, int num)
{
	memcpy(buf, th->dataptr, num);
//	*(th->dataptr) += num;
	th->dataptr += num;

	return num;
}

unsigned char read_byte_mem(track_header_t *th)
{
//	unsigned char data;

	//data = *(th->dataptr);
	//th->dataptr++;
	//return data;
	return *(th->dataptr)++;
}

unsigned int read_int_mem(track_header_t *th)
{
	unsigned short high, low;

	high = read_short_mem(th);
	low = read_short_mem(th);

	return ((high << 16) + low);
}

unsigned short int read_short_mem(track_header_t *th)
{
	unsigned char high, low;

//	high = *(th->dataptr)++;
//	low = *(th->dataptr)++;
	high = *(th->dataptr);
	th->dataptr++;
	low = *(th->dataptr);
	th->dataptr++;
	return MAKEWORD(low, high);
}

// 490
inline unsigned int read_vlq_mem(track_header_t *th)
{
    unsigned int value;
    unsigned char c;
	unsigned char *p = th->dataptr;

    if ((value = *p++) & 0x80)
    {
        value &= 0x7f;
        do
        {
			value = (value << 7) + ((c = *p++) & 0x7f);
        } while (c & 0x80);
    }
	th->dataptr = p;
    /*if ((value = *(th->dataptr)++) & 0x80)
    {
        value &= 0x7f;
        do
        {
			value = (value << 7) + ((c = *(th->dataptr)++) & 0x7f);
        } while (c & 0x80);
    }*/
    return value;
}

inline double GetHRTickCount(void)
{
	double d;

	timeBeginPeriod(1); 
	d = (double) timeGetTime();
	timeEndPeriod(1);
	return d;

	//QueryPerformanceCounter(&LIms_time);
	//return (double) (LIms_time.QuadPart / freq);
}

int analyze_midi(void)
{
	int tracks = mh.num_tracks;
	int i, j;
	int num_events = 0;
	double curtime, starttime;
	double nexttrigger;
	double start_analyze, end_analyze;
	//double start_event, end_event, total_event = 0.0f;
	//double start_vlq, end_vlq, total_vlq = 0.0f;
	char buf[256];
	int first_pass;
	midi_text_t *p;
	static HANDLE standard_image = NULL;

	// Clear tic marks in the song position slider
	SendMessage(GetDlgItem(hwndApp, IDC_SONG_SLIDER), TBM_CLEARTICS, TRUE, 0);

	// Initialize playback parameters
	ms.midi_standard = MIDI_STANDARD_NONE;
	ms.stop_requested = 0;
	//set_tempo(50000 / 1000);
	ms.tempo = 500000 / 1000;
	ms.tick_length = (double) ms.tempo / (double) mh.num_ticks;
	ms.found_note_on = 0;
	ms.first_note_on = 0.0f;
	ms.analyzing = 1;
	//sprintf(buf, "Tempo: %.2f ms/tick, %.0f bpm", ms.tick_length, 60000000.0f / ms.tempo);
	//SetDlgItemText(hwndApp, IDC_TICK, buf);

	// Initialize track parameters
	for (i = 0; i < tracks; i++)
	{
		th[i].name[0] = '\0';
		th[i].num_events = 0;
		th[i].num_events_done = 0;
		th[i].last_channel = -1;
		th[i].last_controller = -1;
		th[i].last_controller_value = -1;
		th[i].last_note_pitch = -1;
		th[i].last_note_velocity = -1;
		th[i].last_program = -1;
		th[i].last_bank = -1;
		th[i].lastcmd = 0;
		th[i].enabled = 1;
		th[i].trigger = 0.0f;
		th[i].dataptr = th[i].data;
		th[i].tracknum = i;
	}

	// Initialize historical channel values
	for (i = 0; i < 16; i++)
	{
		for (j = 0; j < MAX_MIDI_TRACKS; j++)
			ms.channels[i].track_references[j] = -1;
		ms.channels[i].last_controller = -1;
		ms.channels[i].last_controller_value = -1;
		ms.channels[i].last_note_pitch = -1;
		ms.channels[i].last_note_velocity = -1;
		ms.channels[i].last_program = -1;
		ms.channels[i].last_bank = -1;
	}

	// Record starting time
	curtime = starttime = 0.0f;
	ms.starttime = starttime;

	start_analyze = GetHRTickCount();

	// Loop until we've been asked to stop
	first_pass = 1;
	while (!ms.stop_requested)
	{
		// Schedule the next "wakeup time" MAX_MIDI_WAIT milliseconds into the future
		nexttrigger = curtime + MAX_MIDI_WAIT;
		// See if any tracks have pending data
		ms.stop_requested = 1;
		for (i = 0; i < tracks; i++)
		{
			// Continue on to the next track if this track is disabled
			if (!th[i].enabled)
				continue;
			else
				ms.stop_requested = 0;

			// Read the next event's delta time
			if (first_pass)
			{
				th[i].dt = (double) read_vlq_mem(&th[i]) * ms.tick_length;
				th[i].trigger = curtime + th[i].dt;
			}

			// Process MIDI events until one is scheduled for a time in the future
			while (curtime >= th[i].trigger)
			{
				// Process the event for this track
				ms.curtime = curtime;
				//start_event = GetHRTickCount();
				if (process_midi_event(&th[i]))
					return 1;
				//end_event = GetHRTickCount();
				//total_event += end_event - start_event;
				num_events++;
				th[i].num_events++;
				/*if (num_events % 1000 == 0)
				{
					sprintf(buf, "Loaded %s, analyzing... (%d events)", ms.filename, num_events);
					SetDlgItemText(hwndApp, IDC_FILENAME, buf);
				}*/

				// Check for end of track
				if (!(th[i].enabled))
					break;

				if (th[i].dataptr >= (th[i].data + th[i].length))
				{
					th[i].enabled = FALSE;
					break;
				}
				
				// Read the next event's delta time
				//start_vlq = GetHRTickCount();
				th[i].dt = (double) read_vlq_mem(&th[i]) * ms.tick_length;
				//end_vlq = GetHRTickCount();
				//total_vlq += end_vlq - start_vlq;
				th[i].trigger += th[i].dt;
			}
			// Check for end of track
			if (!(th[i].enabled))
				continue;

			// See if this track's trigger is the more recent than the next scheduled trigger
			// If so, make it the next trigger
			if (th[i].trigger < nexttrigger)
				nexttrigger = th[i].trigger;
		}
		first_pass = 0;
		curtime = nexttrigger;
	}
	// Analysis complete
	end_analyze = GetHRTickCount();
	// Store song length and # of events
	ms.song_length = (int) (curtime / 1000.0f);
	ms.num_events = num_events;
	// Set up velocity mod slider
	SendDlgItemMessage(hwndApp, IDC_VELOCITY_SLIDER, TBM_SETRANGE, TRUE, MAKELONG(0, 128));
	SendDlgItemMessage(hwndApp, IDC_VELOCITY_SLIDER, TBM_SETTICFREQ, 32, 0);
	set_mod_velocity(0);
	// Set up pitch mod slider
	SendDlgItemMessage(hwndApp, IDC_PITCH_SLIDER, TBM_SETRANGE, TRUE, MAKELONG(0, 48));
	SendDlgItemMessage(hwndApp, IDC_PITCH_SLIDER, TBM_SETTICFREQ, 12, 0);
	set_mod_pitch(0);
	// Set up tempo slider
	SendDlgItemMessage(hwndApp, IDC_TEMPO_SLIDER, TBM_SETRANGE, TRUE, MAKELONG(0, 500));
	SendDlgItemMessage(hwndApp, IDC_TEMPO_SLIDER, TBM_SETPOS, TRUE, 120);
	SendDlgItemMessage(hwndApp, IDC_TEMPO_SLIDER, TBM_SETTICFREQ, 500 / 10, 0);
	// Set up MIDI event position slider
	//SendDlgItemMessage(hwndApp, IDC_EVENT_SLIDER, TBM_SETTICFREQ, num_events / 10, 0);
	SendDlgItemMessage(hwndApp, IDC_EVENT_SLIDER, TBM_SETRANGEMAX, TRUE, num_events / 10);
	SendDlgItemMessage(hwndApp, IDC_EVENT_SLIDER, TBM_SETPOS, FALSE, 0);
	// Set song length text
	sprintf(buf, "%d:%02d", ms.song_length / 60, ms.song_length % 60);
	SetDlgItemText(hwndApp, IDC_SONG_LENGTH, buf);
	// Set up song position slider
	//SendDlgItemMessage(hwndApp, IDC_SONG_SLIDER, TBM_SETTICFREQ, ms.song_length / 10, 0);
	SendDlgItemMessage(hwndApp, IDC_SONG_SLIDER, TBM_SETRANGE, TRUE, MAKELONG(0, ms.song_length));
	SendDlgItemMessage(hwndApp, IDC_SONG_SLIDER, TBM_SETPOS, FALSE, 0);
	// Set tic marks for text events in the song position slider
	for (p = midi_text_events; p; p = p->next)
	{
		i = (int) p->midi_time;
		if (i && i != ms.song_length)
			SendMessage(GetDlgItem(hwndApp, IDC_SONG_SLIDER), TBM_SETTIC, 0, (LPARAM) (LONG) i);
	}
	// Set up the MIDI standard bitmap
	if (standard_image)
	{
		DeleteObject(standard_image);
		standard_image = NULL;
	}
	// If we haven't decided on a standard, check the filename/path for "MT32" or "MT-32"
	if (ms.midi_standard == MIDI_STANDARD_NONE && 
		(stristr(ms.filename, "MT32") || stristr(ms.filename, "MT-32")))
		ms.midi_standard = MIDI_STANDARD_MT32;
	// Choose an appropriate MIDI standard bitmap
	if (ms.midi_standard == MIDI_STANDARD_NONE)
		ShowWindow(GetDlgItem(hwndApp, IDC_MIDI_LOGO), SW_HIDE);
	else
	{
		switch (ms.midi_standard)
		{
			case MIDI_STANDARD_GM:
				standard_image = LoadImage(ghInstance, MAKEINTRESOURCE(IDB_GMLOGO), IMAGE_BITMAP, 0, 0, 0);
				break;
			case MIDI_STANDARD_GS:
				standard_image = LoadImage(ghInstance, MAKEINTRESOURCE(IDB_GSLOGO), IMAGE_BITMAP, 0, 0, 0);
				break;
			case MIDI_STANDARD_XG:
				standard_image = LoadImage(ghInstance, MAKEINTRESOURCE(IDB_XGLOGO), IMAGE_BITMAP, 0, 0, 0);
				break;
			case MIDI_STANDARD_MT32:
				standard_image = LoadImage(ghInstance, MAKEINTRESOURCE(IDB_MTLOGO), IMAGE_BITMAP, 0, 0, 0);
				break;
		}
		SendDlgItemMessage(hwndApp, IDC_MIDI_LOGO, STM_SETIMAGE, 
			(WPARAM) IMAGE_BITMAP, (LPARAM) standard_image);
		ShowWindow(GetDlgItem(hwndApp, IDC_MIDI_LOGO), SW_SHOW);
	}
	// Decide which instrument names to use
	switch (ms.midi_standard)
	{
		case MIDI_STANDARD_MT32: program_names = mt32_program_names; break;
		default: program_names = gm_program_names;
	}
	// If a text window is open, reinitialize its contents
	if (hwndText)
		SendMessage(hwndText, WM_INITDIALOG, 0, 0);
	// If a tracks window is open, reinitialize its contents
	if (hwndTracks)
		SetupItemsTracksListView(GetDlgItem(hwndTracks, IDC_TRACKS_LIST));
	// If a channels window is open, reinitialize its contents
	if (hwndChannels)
		SetupItemsChannelsListView(GetDlgItem(hwndChannels, IDC_CHANNELS_LIST));
	// If a tracks window is open, reinitialize its contents
	if (hwndSysex)
		SetupItemsSysexListView(GetDlgItem(hwndSysex, IDC_SYSEX_LIST));

	//sprintf(buf, "Analysis of %d events took %.0f milliseconds, %.0f of which were spent in "
	//	"process_midi_event(), and %.0f of which were spent reading VLQ's.  That's an average of %.4f milliseconds per event.", 
	//	num_events, end_analyze - start_analyze, total_event, total_vlq, total_event / (double) num_events);
	//sprintf(buf, "Analysis of %d events took %.0f milliseconds.", 
	//	num_events, end_analyze - start_analyze);
	//MessageBox(hwndApp, buf, "Analysis complete.", 0);

	return 0;
}

void __cdecl playback_thread(void *spointer)
{
	int tracks = mh.num_tracks;
	int i, j, polyphony, elapsed;
	int num_events;
	double curtime, starttime, pausetime, tmptime, timediff, displaytime;
	double nexttrigger;
	char buf[256];
	char *ch;
	int first_pass;
	int tracks_active;
	int seeking = 0;

	// Initialize seeking states
	ms.seeking = 0;
	ms.seek_sliding = 0;
	ms.seek_to = 0.0f;
	/*if (ms.found_note_on && ms.first_note_on > 0.0f)
	{
		ms.seeking = 1;
		ms.seek_to = ms.first_note_on;
	}*/

	// Update status line text
	sprintf(buf, "Playing %s...", extract_filename(ms.filename));
	SetWindowText(hwndStatusBar, buf);

	// Show the default drum kit name on channel 10 if percussion is used by this song..
	// Otherwise blank out the drum kit name
	if (ms.uses_percussion)
		SetDlgItemText(hwndApp, IDC_T0 + 9, "Standard");
	else
		SetDlgItemText(hwndApp, IDC_T0 + 9, "");

	// Init MIDI-out device
	init_midi_out(GetDlgItem(hwndApp, IDC_MIDI_OUT));
	if (!hout)
		return;

	// Set playback thread priority
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	// Disable play button, enable pause and stop buttons
	EnableWindow(GetDlgItem(hwndApp, IDC_PLAY), FALSE);
	EnableWindow(GetDlgItem(hwndApp, IDC_PAUSE), TRUE);
	EnableWindow(GetDlgItem(hwndApp, IDC_STOP), TRUE);

	for (i = 0; i < 16; i++)
	{
		// Set overridden instrument channels
		if (ms.channels[i].program_overridden)
		{
			if (i == 9)
			{
				ch = strchr(drum_kit_names[ms.channels[i].override_program], '-');
				SetDlgItemText(hwndApp, IDC_T0 + i, ch + 2);
			}
			else
				SetDlgItemText(hwndApp, IDC_T0 + i, get_program_name(ms.channels[i].override_program, 0));
			set_channel_program(i, ms.channels[i].override_program, 0);
		}

		// Update channel mutes
		set_channel_mute(i, ms.channels[i].muted);

		// Reset displayed controller to none
		ms.channels[i].displayed_controller = -1;

		// Reset controller locks
		ms.channels[i].lock_controller = 0;
	}

	// Initialize modifiers
	ms.mod_pitch = 0;
	ms.mod_velocity = 0;

	// Initialize loop count
	ms.loop_count = 1;

	while (ms.loop_count--)
	{
BeginPlayback:
		// Initialize tempo
		set_tempo((int) (60000000.0f / ms.tempo));
		/*sprintf(buf, "Tempo: %.2f ms/tick, %.0f bpm", ms.tick_length, 60000000.0f / ms.tempo);
		SetDlgItemText(hwndApp, IDC_TICK, buf);
		SendDlgItemMessage(hwndApp, IDC_TEMPO_SLIDER, TBM_SETPOS, FALSE, (int) (60000000.0f / ms.tempo));
		*/

		// Initialize playback parameters
		num_events = 0;
		//ms.analyzing = 0;
		ms.analyzing = ms.seeking;
		ms.playing = 1;
		ms.paused = 0;
		ms.finished_naturally = FALSE;
		ms.stop_requested = 0;
		ms.tempo = 500000 / 1000;
		ms.tick_length = (double) ms.tempo / (double) mh.num_ticks;
		ms.peak_polyphony = 0;
		ms.current_text[0] = '\0';
		ms.title_displayed = 0;
		SendDlgItemMessage(hwndApp, IDC_POLYPHONY_METER, PBM_SETRANGE, 0, MAKELPARAM(0, ms.peak_polyphony));

		// Reset channel states
		for (i = 0; i < 16; i++)
		{
			// Reset # of notes currently being played on this channel
			ms.channels[i].note_count = 0;
			ms.channels[i].drawn = 1;
			// Reset program values
			ms.channels[i].normal_program = 0;
			//ms.channels[i].override_program = -1;
			// Reset note values
			memset(ms.channels[i].notes, 0, sizeof(ms.channels[i].notes));
			// Reset controller values
			for (j = 0; j < 128; j++)
			{
				ms.channels[i].controllers[j] = -1;
				ms.channels[i].controller_overridden[j] = 0;
			}
			// Reset historical values
			ms.channels[i].last_controller = -1;
			ms.channels[i].last_controller_value = -1;
			ms.channels[i].last_note_pitch = -1;
			ms.channels[i].last_note_velocity = -1;
			ms.channels[i].last_pitch_bend = -1;
		}

		// Initialize track parameters
		for (i = 0; i < tracks; i++)
		{
			th[i].num_events_done = 0;
			//th[i].last_channel = -1;	// Let the analysis set the initial value for this
			th[i].last_controller = -1;
			th[i].last_controller_value = -1;
			th[i].last_note_pitch = -1;
			th[i].last_note_velocity = -1;
			//th[i].last_program = -1;	// Let the analysis set the initial value for this
			//th[i].last_bank = -1;		// Let the analysis set the initial value for this
			th[i].last_pitch_bend = -1;
			th[i].lastcmd = 0;
			th[i].enabled = 1;
			th[i].trigger = 0.0f;
			th[i].dataptr = th[i].data;
			th[i].tracknum = i;
		}

		// Record starting time
		ms.starttime = starttime = curtime = displaytime = GetHRTickCount();
		//sprintf(buf, "starttime = %d", starttime);

		// Loop until we've been asked to stop
		first_pass = 1;
		while (!ms.stop_requested)
		{
			// See if we've been asked to seek and we're not already doing it
			if (ms.seeking && !seeking)
			{
				seeking = 1;
				goto BeginPlayback;		// Aaaah!  A goto!!
			}
			// Get current time
			if (!seeking)
				curtime = GetHRTickCount();
			// Schedule the next "wakeup time" MAX_MIDI_WAIT milliseconds into the future
			nexttrigger = curtime + MAX_MIDI_WAIT;
			// See if any tracks have pending data
			tracks_active = 0;
			for (i = 0; i < tracks; i++)
			{
				// Continue on to the next track if this track is disabled
				if (th[i].enabled)
					tracks_active = 1;
				else
					continue;

				// Disable this track if its data pointer has reached or exceeded the track length
				if (th[i].dataptr >= th[i].data + th[i].length)
				{
					th[i].enabled = FALSE;
					continue;
				}

				// Read the first event's delta time
				//if (th[i].trigger == 0.0f)
				if (first_pass)
				{
					th[i].dt = (double) read_vlq_mem(&th[i]) * ms.tick_length;
					th[i].trigger = curtime + th[i].dt;
				}

				//curtime = GetHRTickCount();

				// Process MIDI events until one is scheduled for a time in the future
				while (curtime >= th[i].trigger)
				{
					// Process the event for this track
					ms.curtime = curtime;
					if (process_midi_event(&th[i]))
					{
						ms.stop_requested = 1;
						break;
					}
					// Increment the global number of events processed, and the number of events for this track
					num_events++;
					th[i].num_events_done++;

					// Check for end of track
					if (!(th[i].enabled))
						break;
					
					// Read the next event's delta time
					th[i].dt = (double) read_vlq_mem(&th[i]) * ms.tick_length;
					th[i].trigger += th[i].dt;
				}
				// Check for end of track
				if (!(th[i].enabled))
					continue;

				// See if this track's trigger is the more recent than the next scheduled trigger
				if (th[i].trigger < nexttrigger)
					nexttrigger = th[i].trigger;
			}
			first_pass = 0;
			if (!seeking)
				curtime = GetHRTickCount();
			//sprintf(buf, "Sleeping for %.2f (now = %.2f, next = %.2f)\n", nexttrigger - curtime, curtime, nexttrigger);
			//OutputDebugString(buf);
			/*sprintf(buf, "%.1f", curtime);
			SetDlgItemText(hwndApp, IDC_CURRENT_TIME, buf);
			sprintf(buf, "%.1f", nexttrigger);
			SetDlgItemText(hwndApp, IDC_TRIGGER_TIME, buf);*/
			// Check to see if a pause has been requested
			if (ms.paused)
			{
				// Turn off the notes being played
				all_notes_off();
				// Save the time we paused at
				pausetime = curtime;
				// Wait until we're unpaused or not playing anymore
				while (1)
				{
					Sleep(100);
					if (!(ms.paused) || !(ms.playing) || ms.stop_requested)
						break;
				}
				// If we've been asked to stop.. break from the main playback loop
				if (!(ms.playing) || ms.stop_requested)
					break;
				// We need to resume.  Correct the start and trigger times to be up-to-date!
				curtime = GetHRTickCount();
				starttime += curtime - pausetime;
				nexttrigger += curtime - pausetime;
				for (i = 0; i < tracks; i++)
					th[i].trigger += curtime - pausetime;
			}

			if (!seeking && curtime - displaytime > 20)
			{
				displaytime = curtime;
				// Set song current MIDI events / total MIDI events text
				sprintf(buf, "%d / %d", num_events, ms.num_events);
				SetDlgItemText(hwndApp, IDC_EVENTS, buf);
				// Set MIDI events position slider
				SendDlgItemMessage(hwndApp, IDC_EVENT_SLIDER, TBM_SETPOS, TRUE, num_events / 10);
				// Set song current time / total time text
				elapsed = (int) ((curtime - starttime) / 1000.0f);
				if (!ms.seek_sliding)
				{
					sprintf(buf, "%d:%02d / %d:%02d", elapsed / 60, elapsed % 60, ms.song_length / 60, ms.song_length % 60);
					SetDlgItemText(hwndApp, IDC_SONG_LENGTH, buf);
					// Set song position slider
					SendDlgItemMessage(hwndApp, IDC_SONG_SLIDER, TBM_SETPOS, TRUE, elapsed);
				}
				// Calculate and display polyphony
				for (polyphony = i = 0; i < 16; i++)
					polyphony += ms.channels[i].note_count;
				if (polyphony > ms.peak_polyphony)
				{
					ms.peak_polyphony = polyphony;
					SendDlgItemMessage(hwndApp, IDC_POLYPHONY_METER, PBM_SETRANGE, 0, MAKELPARAM(0, ms.peak_polyphony));
				}
				sprintf(buf, "%02d/%02d", polyphony, ms.peak_polyphony);
				SetDlgItemText(hwndApp, IDC_POLYPHONY, buf);
				SendDlgItemMessage(hwndApp, IDC_POLYPHONY_METER, PBM_SETPOS, (WPARAM) polyphony, 0);
				// Update the tracks window, if it's open
				if (hwndTracks)
					FillTracksListView(GetDlgItem(hwndTracks, IDC_TRACKS_LIST));
				// Update the channels window, if it's open
				if (hwndChannels)
					FillChannelsListView(GetDlgItem(hwndChannels, IDC_CHANNELS_LIST));
				// Update graphical display
				update_display(NULL);
			}

			// Wait until the next trigger time
			if (seeking)
			{
				curtime = nexttrigger;
				// See if the seek is done
				if (seeking && (curtime - starttime >= ms.seek_to))
				{
					// Seek is done - correct timing values
					tmptime = curtime;
					curtime = GetHRTickCount();
					timediff = tmptime - curtime;
					for (i = 0; i < mh.num_tracks; i++)
						th[i].trigger -= timediff;
					// Calculate corrected start time and elapsed time
					ms.starttime = starttime = curtime - timediff;
					elapsed = (int) (curtime - ms.starttime);
					ms.curtime = curtime;
					// Reset seek state flags
					seeking = ms.seeking = ms.analyzing = 0;
					// Turn all notes off
					all_notes_off();

					// Set channel values
					for (i = 0; i < 16; i++)
					{
						// Set program changes
						if (ms.channels[i].normal_program)
							set_channel_program(i, ms.channels[i].normal_program, ms.channels[i].last_bank);
						// Set controller values
						//for (j = 0; j < 128; j++)
						//	if (ms.channels[i].controllers[j] != -1)
						//		set_channel_controller(i, j, ms.channels[i].controllers[j]);
					}
				}
			}
			else
			{
				//i = 0;
				while (curtime < nexttrigger && !(ms.stop_requested))
				{
#ifdef _DEBUG
					sprintf(buf, "Sleeping for %d s\n", (int) (nexttrigger - curtime));
					OutputDebugString(buf);
#endif
					Sleep((int) (nexttrigger - curtime));
					//Sleep(5);
					curtime = GetHRTickCount();
					//i++;
					//if ((i % 1000) == 0)
					//{
					//	sprintf(buf, "curtime = %.1f, nexttrigger = %.1f, loop count = %d", curtime, nexttrigger, i);
					//	SetWindowText(hwndStatusBar, buf);
					//}
				}
				if (!tracks_active)
				{
					ms.finished_naturally = TRUE;
					break;
				}
			}
		}
		// Break out of looping if a stop was requested
		if (ms.stop_requested)
			break;
		// If the looping checkbox is checked, loop another time
		if (IsDlgButtonChecked(hwndApp, IDC_LOOP))
			ms.loop_count++;
	}
	// PLAYBACK HAS STOPPED
	// Turn off all the notes, reset the MIDI-out device, and close it
	all_notes_off();
	Sleep(50);
	midiOutReset(hout);
	close_midi_out();

	// Reset the track info displays
	for (i = 0; i < 16; i++)
	{
		if (!(ms.channels[i].program_overridden))
			SetDlgItemText(hwndApp, IDC_T0 + i, "");
		memset(ms.channels[i].notes, 0, sizeof(ms.channels[i].notes));
		ms.channels[i].note_count = 0;
		ms.channels[i].displayed_controller = -1;
		ms.channels[i].lock_controller = 0;
		ms.channels[i].drawn = 1;
		//SendDlgItemMessage(hwndApp, IDC_TVU0 + i, PBM_SETPOS, 0, 0);
	}
	// Clear the note displays
	/*hdc = GetDC(hwndApp);
	if (hdc)
	{
		SelectObject(hdc, hNoteBackgroundBrush);
		for (i = 0; i < 16; i++)
			Rectangle(hdc, BAR_X, BAR_Y + i * BAR_VSPACE, BAR_X + BAR_WIDTH, BAR_Y + i * BAR_VSPACE + BAR_HEIGHT);
	}*/
	update_display(NULL);
	// Clear the current text display
	SetDlgItemText(hwndApp, IDC_CURRENT_TEXT, "");
	// Clear the polyphony display
	SetDlgItemText(hwndApp, IDC_POLYPHONY, "");
	SendDlgItemMessage(hwndApp, IDC_POLYPHONY_METER, PBM_SETPOS, 0, 0);
	// Set song current MIDI events / total MIDI events text
	SetDlgItemText(hwndApp, IDC_EVENTS, "");
	// Set MIDI events position slider
	num_events = 0;
	sprintf(buf, "%d / %d", num_events, ms.num_events);
	SetDlgItemText(hwndApp, IDC_EVENTS, buf);
	SendDlgItemMessage(hwndApp, IDC_EVENT_SLIDER, TBM_SETPOS, TRUE, 0);
	// Set song current time / total time text
	elapsed = 0;
	sprintf(buf, "%d:%02d / %d:%02d", elapsed / 60, elapsed % 60, ms.song_length / 60, ms.song_length % 60);
	SetDlgItemText(hwndApp, IDC_SONG_LENGTH, buf);
	// Set song position slider
	SendDlgItemMessage(hwndApp, IDC_SONG_SLIDER, TBM_SETPOS, TRUE, 0);

	// Enable play button, disable pause and stop buttons
	EnableWindow(GetDlgItem(hwndApp, IDC_PLAY), TRUE);
	EnableWindow(GetDlgItem(hwndApp, IDC_PAUSE), FALSE);
	EnableWindow(GetDlgItem(hwndApp, IDC_STOP), FALSE);

	ms.playing = 0;
	PostMessage(hwndApp, WMAPP_DONE_PLAYING, 0, 0);
}

int process_midi_event(track_header_t *th)
{
	char *MThd = "MThd";
	char *MTrk = "MTrk";
	unsigned char *text;
	unsigned char id[4];
	char eventname[128];
	char buf[2048];
	unsigned int track = 0, i, blank, len, endtrack = 0;
	unsigned char lastcmd = 0, cmd, d1, d2, d3, d4, d5;
	signed char sd1;
	unsigned int intd;
	unsigned short pitchbend;
	int channel;
	unsigned char *cmdptr = th->dataptr;		// Pointer to the beginning of this event
	unsigned char *sysexptr;					// Pointer to beginning of sysex data

	// Read the MIDI event command
	cmd = read_byte_mem(th);

	//sprintf(buf, "Track %d: cmd = %d ", th->tracknum, cmd);
	//OutputDebugString(buf);

	// Handle running mode
	if (cmd < 128)
	{
		//sprintf(buf, "running mode ");
		//OutputDebugString(buf);
		th->dataptr--;
		cmd = th->lastcmd;
	}
	else
		th->lastcmd = cmd;

	if (cmd == 0xFF)		// Meta-event
	{
		cmd = read_byte_mem(th);
		len = read_vlq_mem(th);
		if (len > (th->dataptr - th->data) + th->length)
		{
			sprintf(buf, "Meta-event %02X at track %d offset %d has length %d, "
						 "which would exceed track length of %d.\n\n"
						 "Conclusion: %s is corrupt.", 
						 cmd, th->tracknum, cmdptr - th->data, len, th->length, ms.filename);
			MessageBox(hwndApp, buf, "TMIDI Read Error", MB_ICONERROR);
			return 1;
		}
		text = NULL;
		//sprintf(buf, "Meta-event %d/%d\n", cmd, len);
		//OutputDebugString(buf);
		switch (cmd)
		{
			case 0x00:		// Sequence number
				intd = read_short_mem(th);
//				dprintf("Set Sequence number: %d\n", intd);
				break;
			case 0x01:		// Text
			case 0x02:		// Copyright info
			case 0x03:		// Sequence or track name
			case 0x04:		// Track instrument name
			case 0x05:		// Lyric
			case 0x06:		// Marker
			case 0x07:		// Cue point
				strcpy(eventname, midi_text_event_descriptions[cmd]);

				// Read the text
				text = (unsigned char *) malloc(len + 1);
				read_bytes_mem(th, text, len);
				text[len] = '\0';

				// If it's a track name, save it
				if (cmd == 3 || cmd == 4)
				{
					strncpy(th->name, (char *) text, sizeof(th->name));
					th->name[sizeof(th->name) - 1] = '\0';
				}
				else
					if (!th->name[0])
					{
						strncpy(th->name, (char *) text, sizeof(th->name));
						th->name[sizeof(th->name) - 1] = '\0';
					}

				// Save the text in the list of text events
				if (ms.analyzing && !ms.seeking)
					new_midi_text((char *) text, cmd, (ms.curtime - ms.starttime) / 1000.0f, th->tracknum, cmdptr - th->data);
				else
				{
					// Text to speech, heheh
					//if (cmd == 5)
					//	speak("%s", text);
					if (cmd == 5 && ms.current_text[strlen(ms.current_text) - 1] == '-')
						strcat(ms.current_text, (char *) text);
					else
						strncpy(ms.current_text, (char *) text, sizeof(ms.current_text));
					if (!(cmd == 3 && ms.title_displayed) || (cmd == 3 && text[0] == '\"'))
					{
						//if ((((cmd == 5) || (cmd == 6)) && (!ms.title_displayed || ms.curtime - ms.starttime > 500.0f)) || (cmd == 3 && text[0] == '\"') || ((cmd != 3 && (!(ms.title_displayed) || ms.curtime - ms.starttime > 3000.0f)) || ((cmd == 3 || cmd == 6) && (!(ms.title_displayed) || ms.curtime - ms.starttime > 3000.0f))))
						if ((cmd == 5) || (cmd == 6) || (cmd == 3 && text[0] == '\"') || ((cmd != 3 && (!(ms.title_displayed) || ms.curtime - ms.starttime > 3000.0f)) || ((cmd == 3 || cmd == 6) && (!(ms.title_displayed) || ms.curtime - ms.starttime > 3000.0f))))
						{
							if (cmd != 2)
							{
								blank = 1;
								for (i = 0; i < strlen(ms.current_text); i++)
									if (i != ' ')
										blank = 0;
								if (!blank)
								{
									//if (!ms.seeking)	// FIXME: don't go through all text while seeking...
										SetDlgItemText(hwndApp, IDC_CURRENT_TEXT, ms.current_text);
									ms.title_displayed = 1;
								}
							}
						}
					}
				}
				//sprintf(buf, "%s: %s\n", eventname, text);
				//OutputDebugString(buf);
				//fprintf(outfile, "%s: ", eventname);
				//fputs((const char *) text, outfile);
				//fprintf(outfile, "\n");
				break;
			case 0x20:		// MIDI Channel prefix
				d1 = read_byte_mem(th);
				//fprintf(outfile, "MIDI Channel Prefix: %d\n", d1);
				break;
			case 0x21:		// MIDI Port
				d1 = read_byte_mem(th);
				//fprintf(outfile, "MIDI Port: %d\n", d1);
				break;
			case 0x2F:		// End of track
//				//fprintf(outfile, "End of track\n");
				sprintf(buf, "End of track %d\n", th->tracknum);
				OutputDebugString(buf);
				th->enabled = 0;
				endtrack = 1;
				break;
			case 0x51:		// Set tempo
				read_bytes_mem(th, id, 3);
				intd = (id[0] << 16) + (id[1] << 8) + id[2];
				set_tempo(intd);
				break;
			case 0x54:		// SMPTE Offset
				d1 = read_byte_mem(th);
				d2 = read_byte_mem(th);
				d3 = read_byte_mem(th);
				d4 = read_byte_mem(th);
				d5 = read_byte_mem(th);
				//fprintf(outfile, "SMPTE Offset: %d:%02d:%02d.%02d.%02d\n", d1, d2, d3, d4, d5);
				break;
			case 0x58:		// Time signature
				d1 = read_byte_mem(th);
				d2 = read_byte_mem(th);
				d3 = read_byte_mem(th);
				d4 = read_byte_mem(th);
				//fprintf(outfile, "Time signature: %d/%d, %d ticks in metronome click, %d 32nd notes to quarter note\n", d1, d2, d3, d4);
				break;
			case 0x59:		// Key signature
				sd1 = read_byte_mem(th);
				d2 = read_byte_mem(th);
				//fprintf(outfile, "Key signature: ");
//				if (sd1 < 0)
					//fprintf(outfile, "%d flats, ", 0 - sd1);
//				else
//					if (sd1 > 0)
						//fprintf(outfile, "%d sharps, ", sd1);
//					else
						//fprintf(outfile, "Key of C, ");
				//fprintf(outfile, d2 ? "minor\n" : "major\n");
				break;
			case 0x7F:		// Sequencer-specific information
				//fprintf(outfile, "Sequencer-specific information, %d bytes: ", len);
				for (i = 0; i < len; i++)
					read_byte_mem(th);
				//fputc(read_byte_mem(th), outfile);
				//fprintf(outfile, "\n");
				break;
			default:		// Unknown
				//fprintf(outfile, "Meta-event, unknown command %X length %d: ", cmd, len);
				for (i = 0; i < len; i++)
					read_byte_mem(th);
//					fputc(read_byte_mem(th), outfile);
				//fprintf(outfile, "\n");
		}
		if (text)
		{
			free(text);
			text = NULL;
		}
	}
	else
	{
		//sprintf(buf, "cmd = %d", cmd);
		//OutputDebugString(buf);
		channel = LONYBBLE(cmd);
		// *** cool symmetric part follows: ***
		// Save this channel number in the track struct for historical purposes
		th->last_channel = channel;
		// Save this track number in the channel struct for historical purposes
		ms.channels[channel].track_references[th->tracknum] = 1;
		switch (HINYBBLE(cmd))	// Normal event
		{
			case 0x08: // Note off
				d1 = read_byte_mem(th);
				d2 = read_byte_mem(th);
				if (!ms.analyzing)
				{
					note_on(FALSE, d1, d2, channel);
					//midiOutShortMsg(hout, MAKELONG(MAKEWORD(cmd, d1), MAKEWORD(d2, 0)));
					/*sprintf(buf, "Note off, d1 = %d, d2 = %d\n", d1, d2);
					SetDlgItemText(hwndApp, IDC_FILENAME, buf);
					OutputDebugString(buf);*/
				}
				//fprintf(outfile, "Note Off, note %d velocity %d\n", d1, d2);
				break;
			case 0x09: // Note on
				d1 = read_byte_mem(th);
				d2 = read_byte_mem(th);
				if (!ms.analyzing)
				{
					th->last_note_pitch = d1;
					th->last_note_velocity = d2;
					note_on(TRUE, d1, d2, channel);
					//midiOutShortMsg(hout, MAKELONG(MAKEWORD(cmd, d1), MAKEWORD(d2, 0)));
					/*sprintf(buf, "Note on, d1 = %d, d2 = %d\n", d1, d2);
					SetDlgItemText(hwndApp, IDC_FILENAME, buf);
					OutputDebugString(buf);*/
				}
				if (!ms.found_note_on)
				{
					ms.found_note_on = 1;
					ms.first_note_on = ms.curtime;
				}
				//fprintf(outfile, "Note On, note %d velocity %d\n", d1, d2);
				break;
			case 0x0A: // Key after-touch
				d1 = read_byte_mem(th);
				d2 = read_byte_mem(th);
				if (!ms.analyzing)
					midiOutShortMsg(hout, MAKELONG(MAKEWORD(cmd, d1), MAKEWORD(d2, 0)));
				//fprintf(outfile, "Key After-touch, note %d velocity %d\n", d1, d2);
				break;
			case 0x0B: // Control Change
				d1 = read_byte_mem(th);
				d2 = read_byte_mem(th);
				// Save the controller value
				if (ms.seeking)
					ms.channels[channel].controllers[d1] = d2;
				if (!ms.analyzing)
				{
					ms.channels[channel].last_controller = th->last_controller = d1;
					ms.channels[channel].last_controller_value = th->last_controller_value = d2;
					if (d1 == 0)
						ms.channels[channel].last_bank = th->last_bank = d2;
					if (!ms.channels[channel].lock_controller)
						ms.channels[channel].displayed_controller = d1;		// Automatically select which controller to display
					if (!ms.channels[channel].controller_overridden[d1])
					{
						ms.channels[channel].controllers[d1] = d2;			// Update controller value
						midiOutShortMsg(hout, MAKELONG(MAKEWORD(cmd, d1), MAKEWORD(d2, 0)));
					}
				}
				//fprintf(outfile, "Control Change, controller %d value %d\n", d1, d2);
				break;
			case 0x0C: // Program Change
				d1 = read_byte_mem(th);
				if (!ms.analyzing || ms.seeking)
				{
					ms.channels[channel].last_program = th->last_program = d1;
					ms.channels[channel].normal_program = d1;
					if (!(ms.channels[channel].program_overridden)) // && !ms.seeking)
						set_channel_program(channel, d1, ms.channels[channel].last_bank);
				}
				//fprintf(outfile, "Program Change, program %d\n", d1);
				break;
			case 0x0D: // Channel after-touch
				d1 = read_byte_mem(th);
				if (!ms.analyzing)
					midiOutShortMsg(hout, MAKELONG(MAKEWORD(cmd, d1), MAKEWORD(0, 0)));
				//fprintf(outfile, "Channel After-touch, channel %d\n", d1);
				break;
			case 0x0E: // Pitch wheel
				intd = read_short_mem(th);
				d2 = LOBYTE(intd);
				d1 = HIBYTE(intd);
				pitchbend = (unsigned short) d2;
				pitchbend <<= 7;
				pitchbend |= (unsigned short) d1;
				if (!ms.analyzing)
				{
					midiOutShortMsg(hout, MAKELONG(MAKEWORD(cmd, d1), MAKEWORD(d2, 0)));
					ms.channels[channel].last_pitch_bend = th->last_pitch_bend = (signed int) pitchbend - MAX_PITCH_BEND;
				}
				else
				{
					// Keep track of the highest pitch bend value used in this song
					if (abs((signed int) pitchbend - MAX_PITCH_BEND) > ms.highest_pitch_bend)
						ms.highest_pitch_bend = abs((signed int) pitchbend - MAX_PITCH_BEND);
				}
				//fprintf(outfile, "Pitch Wheel, amount %d\n", intd);
				break;
			case 0x0F: // System message
				switch (LONYBBLE(cmd))
				{
					case 0x02:
						d1 = read_byte_mem(th);
						d2 = read_byte_mem(th);
						intd = (d2 << 8) + d1;
						//fprintf(outfile, "System Message: Song position: %d\n", intd);
					case 0x03:
						d1 = read_byte_mem(th);
						//fprintf(outfile, "System Message: Song select: %d\n", d1);
					case 0x06:
						//fprintf(outfile, "System Message: Tune request\n");
						break;
					case 0x00:
					case 0x07:	// SYSEX data
						len = read_vlq_mem(th);
						if (len > (th->dataptr - th->data) + th->length)
						{
							sprintf(buf, "Sysex data at track %d offset %d has length %d, "
										 "which would exceed track length of %d.\n\n"
										 "Conclusion: %s is corrupt.", 
										 th->tracknum, cmdptr - th->data, len, th->length, ms.filename);
							MessageBox(hwndApp, buf, "TMIDI Read Error", MB_ICONERROR);
							return 1;
						}
						sysexptr = th->dataptr;
						if (ms.analyzing && !ms.seeking)
							new_midi_sysex(sysexptr, len, (ms.curtime - ms.starttime) / 1000.0f, th->tracknum, cmdptr - th->data, channel);
						//fprintf(outfile, "SysEx Data, length %d: %02X ", len, cmd);
						th->dataptr += len;
						/*for (i = 0; i < len; i++)
							read_byte_mem(th);*/
						if (!ms.analyzing)
						{
							// Interpret the sysex data, in case we need it (like MT-32)
							interpret_sysex(sysexptr, len);
							// Don't send sysex when seeking on the MT-32
							if (!(ms.midi_standard == MIDI_STANDARD_MT32 && ms.seeking))
								output_sysex_data(channel, sysexptr, len);
						}
							//fprintf(outfile, "%02X ", read_byte_mem(th));
						//fprintf(outfile, "\n");
						break;
					case 0x08:
						//fprintf(outfile, "System Message: Timing clock used when synchronization is required.\n");
						break;
					case 0x0A:
						//fprintf(outfile, "System Message: Start current sequence\n");
						break;
					case 0x0B:
						//fprintf(outfile, "System Message: Continue a stopped sequence where left off\n");
						break;
					case 0x0C:
						//fprintf(outfile, "System Message: Stop a sequence\n");
						break;
					//default:
						//fprintf(outfile, "System Message: Unknown (%d)\n", LONYBBLE(cmd));
				}
				break;
			default:
				//fprintf(outfile, "Unknown Message (%d)\n", HINYBBLE(cmd));
				read_byte_mem(th);
		}
		//OutputDebugString("\n");
	}

	return 0;
}

void read_registry_settings(void)
{
	unsigned long i;

	i = sizeof(midi_in_cb);
	RegQueryValueEx(key, "midi_in_cb", 0, NULL, (BYTE *) &midi_in_cb, (unsigned long *) &i);
	i = sizeof(midi_out_cb);
	RegQueryValueEx(key, "midi_out_cb", 0, NULL, (BYTE *) &midi_out_cb, (unsigned long *) &i);
	i = sizeof(alwaysCheckAssociations);
	RegQueryValueEx(key, "alwaysCheckAssociations", 0, NULL, (BYTE *) &alwaysCheckAssociations, (unsigned long *) &i);
	i = sizeof(appRectSaved);
	RegQueryValueEx(key, "appRectSaved", 0, NULL, (BYTE *) &appRectSaved, (unsigned long *) &i);
	i = sizeof(textRectSaved);
	RegQueryValueEx(key, "textRectSaved", 0, NULL, (BYTE *) &textRectSaved, (unsigned long *) &i);
	i = sizeof(tracksRectSaved);
	RegQueryValueEx(key, "tracksRectSaved", 0, NULL, (BYTE *) &tracksRectSaved, (unsigned long *) &i);
	i = sizeof(channelsRectSaved);
	RegQueryValueEx(key, "channelsRectSaved", 0, NULL, (BYTE *) &channelsRectSaved, (unsigned long *) &i);
	i = sizeof(tracksRectSaved);
	RegQueryValueEx(key, "sysexRectSaved", 0, NULL, (BYTE *) &sysexRectSaved, (unsigned long *) &i);
	i = sizeof(appRect);
	RegQueryValueEx(key, "appRect", 0, NULL, (BYTE *) &appRect, (unsigned long *) &i);
	i = sizeof(textRect);
	RegQueryValueEx(key, "textRect", 0, NULL, (BYTE *) &textRect, (unsigned long *) &i);
	i = sizeof(tracksRect);
	RegQueryValueEx(key, "tracksRect", 0, NULL, (BYTE *) &tracksRect, (unsigned long *) &i);
	i = sizeof(channelsRect);
	RegQueryValueEx(key, "channelsRect", 0, NULL, (BYTE *) &channelsRect, (unsigned long *) &i);
	i = sizeof(sysexRect);
	RegQueryValueEx(key, "sysexRect", 0, NULL, (BYTE *) &sysexRect, (unsigned long *) &i);
	i = sizeof(genericTextRect);
	RegQueryValueEx(key, "genericTextRect", 0, NULL, (BYTE *) &genericTextRect, (unsigned long *) &i);
}

void write_registry_settings(void)
{
	RegSetValueEx(key, "midi_in_cb", 0, REG_DWORD, (CONST BYTE *) &midi_in_cb, sizeof(midi_in_cb));
	RegSetValueEx(key, "midi_out_cb", 0, REG_DWORD, (CONST BYTE *) &midi_out_cb, sizeof(midi_out_cb));
	RegSetValueEx(key, "alwaysCheckAssociations", 0, REG_DWORD, (CONST BYTE *) &alwaysCheckAssociations, sizeof(alwaysCheckAssociations));
	RegSetValueEx(key, "appRectSaved", 0, REG_DWORD, (CONST BYTE *) &appRectSaved, sizeof(appRectSaved));
	RegSetValueEx(key, "textRectSaved", 0, REG_DWORD, (CONST BYTE *) &textRectSaved, sizeof(textRectSaved));
	RegSetValueEx(key, "tracksRectSaved", 0, REG_DWORD, (CONST BYTE *) &tracksRectSaved, sizeof(tracksRectSaved));
	RegSetValueEx(key, "channelsRectSaved", 0, REG_DWORD, (CONST BYTE *) &channelsRectSaved, sizeof(channelsRectSaved));
	RegSetValueEx(key, "sysexRectSaved", 0, REG_DWORD, (CONST BYTE *) &sysexRectSaved, sizeof(sysexRectSaved));
	RegSetValueEx(key, "appRect", 0, REG_BINARY, (CONST BYTE *) &appRect, sizeof(appRect));
	RegSetValueEx(key, "textRect", 0, REG_BINARY, (CONST BYTE *) &textRect, sizeof(textRect));
	RegSetValueEx(key, "tracksRect", 0, REG_BINARY, (CONST BYTE *) &tracksRect, sizeof(tracksRect));
	RegSetValueEx(key, "channelsRect", 0, REG_BINARY, (CONST BYTE *) &channelsRect, sizeof(channelsRect));
	RegSetValueEx(key, "sysexRect", 0, REG_BINARY, (CONST BYTE *) &sysexRect, sizeof(sysexRect));
	RegSetValueEx(key, "genericTextRect", 0, REG_BINARY, (CONST BYTE *) &genericTextRect, sizeof(genericTextRect));
}

// Sets the program override for a given channel
void set_program_override(int channel, int override, int program)
{
	if (override)
	{
		// Set a program override
		ms.channels[channel].program_overridden = 1;
		ms.channels[channel].override_program = program;
		set_channel_program(channel, program, 0);
	}
	else
	{
		// Remove a program override
		if (ms.channels[channel].program_overridden)
		{
			ms.channels[channel].program_overridden = 0;
			set_channel_program(channel, ms.channels[channel].normal_program, ms.channels[channel].last_bank);
		}
	}
}

// Sets the program for a given channel
void set_channel_program(int channel, int program, int bank)
{
	assert(channel < 16);
	assert(program < 128);

	if (hout)
		midiOutShortMsg(hout, MAKELONG(MAKEWORD(MAKEBYTE(channel, 0x0C), program), MAKEWORD(0, 0)));

	if (channel != 9)
		SetDlgItemText(hwndApp, IDC_T0 + channel, get_program_name(program, bank));
	else
		SetDlgItemText(hwndApp, IDC_T0 + channel, get_drum_kit_name(program));
}

char *get_drum_kit_name(int program)
{
	int i;
	char *ch;
	static char buf[256];

	for (i = 0; i < sizeof(drum_kit_names) / sizeof(char *); i++)
		if (atoi(drum_kit_names[i]) == program)
			break;
	if (i == sizeof(drum_kit_names) / sizeof(char *))
		sprintf(buf, "Unknown drum kit (%d)", program);
	else
	{
		ch = strchr(drum_kit_names[i], '-');
		strcpy(buf, ch + 2);
	}

	return buf;
}

char *get_sysex_manufacturer_name(int id)
{
	int i;
	char *ch;
	static char buf[256];

	for (i = 0; i < sizeof(sysex_manufacturer_names) / sizeof(char *); i++)
		if (atoi(sysex_manufacturer_names[i]) == id)
			break;
	if (i == sizeof(sysex_manufacturer_names) / sizeof(char *))
		sprintf(buf, "Unknown (%d)", id);
	else
	{
		ch = strchr(sysex_manufacturer_names[i], '-');
		strcpy(buf, ch + 2);
	}

	return buf;
}

void set_channel_controller(unsigned char channel, unsigned char controller, unsigned char value)
{
	if (hout)
		midiOutShortMsg(hout, MAKELONG(MAKEWORD(MAKEBYTE(channel, 0x0B), controller), MAKEWORD(value, 0)));
}

// Updates the volume for a given note on a given channel
void update_note_volume(char channel, char note, char volume)
{
	//int i;
	if (volume)
	{
		// Note on
		if (!(ms.channels[channel].notes[note]))
			ms.channels[channel].note_count++;
	}
	else
	{
		// Note off
		if (ms.channels[channel].notes[note])
			ms.channels[channel].note_count--;
	}
	// Store the value
	ms.channels[channel].notes[note] = volume;
	//SendDlgItemMessage(hwndApp, IDC_TVU0 + channel, WM_USER + 9, 0, RGB(rand() % 256, rand() % 256, rand() % 256));
	//SendDlgItemMessage(hwndApp, IDC_TVU0 + channel, PBM_SETPOS, (WPARAM) note, 0);
}

// Update the background for a single channel
void update_channel_background(HDC hdc, int i, channel_state_t *c)
{
	int w;
	HPEN holdpen = NULL;
	RECT txtrect;
	static HPEN whitepen = (struct HPEN__ *) GetStockObject(WHITE_PEN);
	static HPEN blackpen = (struct HPEN__ *) GetStockObject(BLACK_PEN);
	static char buf[256];
	int v;

	// Get the controller value
	if (c->displayed_controller != -1)
	{
		v = c->controllers[c->displayed_controller];
		// Calculate the width of the displayed controller bar
		w = (int) ((((float) v / 127.0)) * BAR_WIDTH);
		// Draw the displayed controller value bar
		SelectObject(hdc, hControllerBrush);
		Rectangle(hdc, (BAR_X - 1), BAR_Y + i * BAR_VSPACE, BAR_X + w, BAR_Y + i * BAR_VSPACE + BAR_HEIGHT);
		// Draw the bar background
		//if (w)
		//	w++;
		SelectObject(hdc, hNoteBackgroundBrush);
		Rectangle(hdc, (BAR_X - 1) + w, BAR_Y + i * BAR_VSPACE, BAR_X + BAR_WIDTH, BAR_Y + i * BAR_VSPACE + BAR_HEIGHT);
		// Draw a line to cover up the black line common to both rectangles (it contrasts too much)
		if (w && w < BAR_WIDTH)
		{
			holdpen = (HPEN) GetCurrentObject(hdc, OBJ_PEN);
			SelectObject(hdc, hNoteBackgroundPen);
			MoveToEx(hdc, (BAR_X - 1) + w, BAR_Y + i * BAR_VSPACE + 1, NULL);
			LineTo(hdc, (BAR_X - 1) + w, BAR_Y + i * BAR_VSPACE + BAR_HEIGHT - 1);
			SelectObject(hdc, holdpen);
		}
		// Draw some text for the controller name
		SetBkMode(hdc, TRANSPARENT);
		//SetTextColor(hdc, RGB(255, 255, 255));
		SetTextColor(hdc, RGB(144, 144, 144));
		txtrect.left = (BAR_X - 1) + 2;
		txtrect.top = BAR_Y + i * BAR_VSPACE + 2;
		txtrect.bottom = BAR_Y + i * BAR_VSPACE + BAR_HEIGHT - 1;
		txtrect.right = (BAR_X - 1) + 2 + BAR_WIDTH - 2;
		SelectObject(hdc, hControllerFont);
		//if (v != -1)
		//	sprintf(buf, "%s (%d)", controller_names[c->displayed_controller], c->controllers[c->displayed_controller]);
		//else
			sprintf(buf, "%s", controller_names[c->displayed_controller]);
		DrawText(hdc, buf, strlen(buf), &txtrect, DT_LEFT | DT_VCENTER); // DT_END_ELLIPSIS
//		SelectObject(hdc, holdpen);
	}
	else
	{
		SelectObject(hdc, hNoteBackgroundBrush);
		Rectangle(hdc, (BAR_X - 1), BAR_Y + i * BAR_VSPACE, BAR_X + BAR_WIDTH, BAR_Y + i * BAR_VSPACE + BAR_HEIGHT);
	}
}

// Update the graphical display
void update_display(HDC hdc)
{
	static int first = 1;
	static HWND channel_hwnd[16];
	static RECT channel_rect[16];
	static int width, height;
	int hdc_specified = 0;
	int i, j, note, vol, x, y, r, g, b, h, pb;
	HPEN hpen, holdpen;
	channel_state_t *c;

	if (hdc)
		hdc_specified = 1;

	// Loop through the MIDI channels
	for (i = 0; i < 16; i++)
	{
		c = &ms.channels[i];
		// Distinguish between channels with active notes versus silent channels
		if (c->note_count)
		{
			// This channel has at least one note playing.
			if (!hdc)
				hdc = GetDC(hwndApp);
			// Has anything been drawn for this channel?
			if (c->drawn)
			{
				// Yes, get rid of it!
				c->drawn = 0;
				// Update the channel background
				update_channel_background(hdc, i, c);
			}
			// Loop through the notes on this channel
			for (j = 0; j < 128; j++)
			{
				// Get the volume for this note (0 means the note is not being played)
				vol = c->notes[j];
				if (vol)
				{
					// This note is being played.  Figure out where in the bar it should 
					// be drawn based on its note number (pitch).
					//
					// Since notes can range from 0 to 127 in pitch, but they're usually 
					// not played at the extremes, I assume a note's pitch is from 24 to 96 
					// when calculating its position.  If the calculated position is outside
					// the bar, the loop shifts it up or down by an octive until it's in range.
					note = j;
					do
					{
						x = BAR_X + (note - 24) * BAR_WIDTH / (96 - 24);
						if (x > BAR_X + BAR_WIDTH - 1)
							note -= 12;
						if (x < BAR_X)
							note += 12;
					}
					while (x > BAR_X + BAR_WIDTH - 1 || x < BAR_X);
					// Mark this channel as having something drawn for it.
					c->drawn = 1;
					// Pick color based on instrument
					if (i == 9)
						r = g = b = 0;
					else
						hsv_to_rgb((float) c->normal_program * 360.0f / 128.0f, 1.0f, 1.0f, &r, &g, &b);
					// Darken the color and keep it within bounds
					r -= 64;
					if (r < 0)
						r = 0;
					g -= 64;
					if (g < 0)
						g = 0;
					b -= 64;
					if (b < 0)
						b = 0;
					// Create a pen for this color and select it into the DC
					holdpen = (HPEN) GetCurrentObject(hdc, OBJ_PEN);
					hpen = CreatePen(PS_SOLID, 1, RGB(r, g, b));
					SelectObject(hdc, hpen);
					// Calculate the height of the bar
					h = (int) (MAX_NOTE_HEIGHT * (float) vol / 127.0);
					// Calculate the top of the bar
					y = BAR_Y + i * BAR_VSPACE + 1 + (MAX_NOTE_HEIGHT - h); // / 2;
					// Draw the note bar differently depending on whether or not this channel is being pitch-bended
					if (c->last_pitch_bend == 0 || c->last_pitch_bend == -1)
					{
						// Draw a straight line going down
						MoveToEx(hdc, x, y, NULL);
						LineTo(hdc, x, y + h);
					}
					else
					{
						// Calculate the horizontal offset of the pitch bend
						if (ms.highest_pitch_bend)
							pb = (int) ((BAR_WIDTH / 10) * (float) c->last_pitch_bend / (float) ms.highest_pitch_bend);
						else
							pb = (int) ((BAR_WIDTH / 10) * (float) c->last_pitch_bend / MAX_PITCH_BEND);
						// Keep it within the boundaries of the bar
						if (x + pb >= BAR_X + BAR_WIDTH - 2)
							pb = BAR_X + BAR_WIDTH - x - 2;
						if (x + pb <= BAR_X)
							pb = BAR_X - x;
						// Draw a line going down half way to the center of the bar
						MoveToEx(hdc, x + pb, y, NULL);
						LineTo(hdc, x, y + h / 2);
						// Draw a line going down and out for the second half of the bar
						LineTo(hdc, x + pb, y + h);
					}
					// Select the old pen back into the DC and delete the temp color pen
					SelectObject(hdc, holdpen);
					DeleteObject(hpen);
				}
			}
		}
		else
		{
			// The channel has no active notes.  Does it have anything drawn?
			if (c->drawn)
			{
				// Yes?  Clear it then, since nothing's playing now.
				if (!hdc)
					hdc = GetDC(hwndApp);
				c->drawn = 0;
				// Update the channel background
				update_channel_background(hdc, i, c);
			}
		}
	}
	if (hdc && !hdc_specified)
		ReleaseDC(hwndApp, hdc);
}

/* hsv_to_rgb:
 *  Converts from HSV colorspace to RGB values.
 */
void hsv_to_rgb(float h, float s, float v, int *r, int *g, int *b)
{
   float f, x, y, z;
   int i;

   v *= 255.0f;

   if (s == 0.0f) {
      *r = *g = *b = (int)v;
   }
   else {
      while (h < 0.0f)
	  h += 360.0f;
      h = (float) fmod(h, 360) / 60.0f;
      i = (int) h;
      f = h - (float) i;
      x = v * (1.0f - s);
      y = v * (1.0f - (s * f));
      z = v * (1.0f - (s * (1.0f - f)));

      switch (i) {
	 case 0: *r = (int) v; *g = (int) z; *b = (int) x; break;
	 case 1: *r = (int) y; *g = (int) v; *b = (int) x; break;
	 case 2: *r = (int) x; *g = (int) v; *b = (int) z; break;
	 case 3: *r = (int) x; *g = (int) y; *b = (int) v; break;
	 case 4: *r = (int) z; *g = (int) x; *b = (int) v; break;
	 case 5: *r = (int) v; *g = (int) x; *b = (int) y; break;
      }
   }
}

// Adds a new node to the list of MIDI text events
midi_text_t *new_midi_text(char *midi_text, int text_type, double midi_time, int track, int track_offset)
{
	midi_text_t *p, *n;
	char *ch;

	// Allocate memory for the new node
	n = (midi_text_t *) calloc(1, sizeof(midi_text_t));

	// If the list is empty.. add this as the top node
	if (!midi_text_events)
		midi_text_events = n;
	else
	{
		// Otherwise find the end of the list and add this node to the end
		for (p = midi_text_events; p->next; p = p->next);
		p->next = n;
	}

	// Initialize data for this node
	n->text_type = text_type;
	n->midi_time = midi_time;
	n->track = track;
	n->track_offset = track_offset;
	n->next = NULL;

	if (midi_text)
	{
		// Sneaky trick to detect MT-32 MIDI files!
		if (ms.midi_standard == MIDI_STANDARD_NONE && strstr(midi_text, "MT-32"))
			ms.midi_standard = MIDI_STANDARD_MT32;

		// Cool trick to break up lines with CR/LF pairs
		// Search for a CR or LF
		ch = strchr(midi_text, 13);
		if (!ch)
			ch = strchr(midi_text, 10);
		// Found one?  Good.. let's break up the line into multiple text entries!
		if (ch)
		{
			// Terminate the string at this character and move to the next character
			*ch++ = '\0';
			// See if there's another CR/LF to seek past
			if (*ch == 13 || *ch == 10)
				ch++;
			// Make a new text entry with what we just seeked past
			new_midi_text(ch, text_type, midi_time, track, track_offset);
		}

		n->midi_text = strdup(midi_text);
	}

	// Return a pointer to the new node
	return n;
}

// Removes all nodes in the list of MIDI text events
void kill_all_midi_text(void)
{
	midi_text_t *p = midi_text_events, *next;

	while (p)
	{
		if (p->midi_text)
			free(p->midi_text);
		next = p->next;
		free(p);
		p = next;
	}
	midi_text_events = NULL;
}

// Adds a new node to the list of MIDI sysex events
midi_sysex_t *new_midi_sysex(unsigned char *data, int length, double midi_time, int track, int track_offset, int channel)
{
	midi_sysex_t *p, *n;

	// Allocate memory for the new node
	n = (midi_sysex_t *) calloc(1, sizeof(midi_sysex_t));

	// If the list is empty.. add this as the top node
	if (!midi_sysex_events)
		midi_sysex_events = n;
	else
	{
		// Otherwise find the end of the list and add this node to the end
		for (p = midi_sysex_events; p->next; p = p->next);
		p->next = n;
	}

	// See if the data is a MIDI reset
	check_midi_standard(data);

	// Initialize data for this node
	n->data = data;
	n->length = length;
	n->midi_time = midi_time;
	n->track = track;
	n->track_offset = track_offset;
	n->channel = channel;
	n->next = NULL;

	// Return a pointer to the new node
	return n;
}

// Removes all nodes in the list of MIDI sysex events
void kill_all_midi_sysex(void)
{
	midi_sysex_t *p = midi_sysex_events, *next;

	while (p)
	{
		next = p->next;
		free(p);
		p = next;
	}
	midi_sysex_events = NULL;
}

// Dialog for MIDI text window
INT_PTR CALLBACK TextDlg(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	char buf[2048];
	HWND hwndLB;
	midi_text_t *p;
	int num_text_events;
	int i, textlen;
	char *txtbuf;
	midi_text_t *t;

	switch (iMsg)
	{
		case WM_INITDIALOG:
			hwndText = hDlg;
			//SetForegroundWindow(hDlg);
			num_text_events = 0;

			hwndLB = GetDlgItem(hDlg, IDC_TEXT_LIST);
			SendMessage(hwndLB, LB_RESETCONTENT, 0, 0);
			for (p = midi_text_events; p; p = p->next)
			{
				sprintf(buf, "[%d:%02d] %s: %s", 
					(int) p->midi_time / 60, (int) p->midi_time % 60,
					midi_text_event_descriptions[p->text_type], p->midi_text);
				i = SendMessage(hwndLB, LB_ADDSTRING, 0, (LPARAM) buf);
				SendMessage(hwndLB, LB_SETITEMDATA, i, (LPARAM) p);
				num_text_events++;
			}
			//SendMessage(hwndLB, LB_SETTOPINDEX, (WPARAM) num_text_events - 1, 0);
			if (textRectSaved)
				SetWindowPos(hDlg, 0, textRect.left, textRect.top, textRect.right - textRect.left, textRect.bottom - textRect.top, SWP_NOZORDER);

			//SendMessage(hwndLB, LB_SETLOCALE, MAKELCID(MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT), SORT_DEFAULT), 0);
			//SendMessage(hwndLB, WM_SETFONT, (WPARAM) hJapaneseFont, 0);

			return TRUE;

		case WM_COMMAND:
			if (HIWORD(wParam) == LBN_DBLCLK)
			{
				// List box was double-clicked
				i = SendMessage((HWND) lParam, LB_GETCURSEL, 0, 0);
				if (i != LB_ERR)
				{
					t = (midi_text_t *) SendMessage((HWND) lParam, LB_GETITEMDATA, i, 0);
					if ((int) t != LB_ERR)
					{
						copy_to_clipboard(t->midi_text);
					}
					/*textlen = SendMessage((HWND) lParam, LB_GETTEXTLEN, i, 0);
					if (textlen != LB_ERR)
					{
						txtbuf = (char *) malloc(textlen + 2);
						if (txtbuf)
						{
							if (SendMessage((HWND) lParam, LB_GETTEXT, i, (LPARAM) txtbuf) != LB_ERR)
							{
								copy_to_clipboard(txtbuf);
								free(txtbuf);
							}
						}
					}*/
				}
			}
			break;

		case WM_SIZE:
			MoveWindow(GetDlgItem(hDlg, IDC_TEXT_LIST), 0, 0, 
				LOWORD(lParam), HIWORD(lParam), TRUE);
			break;

		case WM_CLOSE:
			GetWindowRect(hDlg, &textRect);
			textRectSaved = 1;
			hwndText = 0;
			DestroyWindow(hDlg);
			break;
	}

	return FALSE;
}

// Dialog for generic text window
INT_PTR CALLBACK GenericTextDlg(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	switch (iMsg)
	{
		case WM_INITDIALOG:
		{
			if (genericTextRectSaved)
				SetWindowPos(hDlg, 0, genericTextRect.left, genericTextRect.top, genericTextRect.right - genericTextRect.left, genericTextRect.bottom - genericTextRect.top, SWP_NOZORDER);

			return TRUE;
		}
		case WM_SIZE:
		{
			MoveWindow(GetDlgItem(hDlg, IDC_GENERIC_TEXT), 0, 0, 
				LOWORD(lParam), HIWORD(lParam), TRUE);
			break;
		}
		case WM_CLOSE:
		{
			GetWindowRect(hDlg, &genericTextRect);
			genericTextRectSaved = 1;
			DestroyWindow(hDlg);
			break;
		}
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDOK:
				case IDCANCEL:
					PostMessage(hDlg, WM_CLOSE, 0, 0);
					break;
			}
			break;
	}

	return FALSE;
}

void set_tempo(int new_tempo)
{
	double old_tick_length;
	char buf[256];
	int i;
	static int last_tempo = 0;

	// Calculate new tick length
	old_tick_length = ms.tick_length;
	ms.tick_length = ((double) (new_tempo / 1000) / (double) mh.num_ticks);

	if (new_tempo == last_tempo)
		return;

	last_tempo = new_tempo;

	if (!ms.analyzing || ms.seeking)
	{
		// Update tick length display on the main dialog
		sprintf(buf, "Tempo: %.2f ms/tick, %.0f bpm", ms.tick_length, 60000000.0f / new_tempo);
		SetDlgItemText(hwndApp, IDC_TICK, buf);
		SendDlgItemMessage(hwndApp, IDC_TEMPO_SLIDER, TBM_SETPOS, TRUE, (int) (60000000.0f / new_tempo));
	}

	// Output some debug info summarizing this tempo change
	//sprintf(buf, "(%d / 1000) / %d = %.2f", intd, mh.num_ticks, ms.tick_length);
	//OutputDebugString(buf);
	//sprintf(buf, "Set tempo: %d (%02X %02X %02X)\n", intd, id[0], id[1], id[2]);
	//OutputDebugString(buf);

	// Correct outstanding track triggers on each track
	for (i = 0; i < mh.num_tracks; i++)
	{
		if (!(th[i].enabled))
			continue;
		th[i].dt = (th[i].trigger - ms.curtime) / old_tick_length * ms.tick_length;
		th[i].trigger = ms.curtime + th[i].dt;
		//sprintf(buf, "Out of %.1f ticks, %.1f ticks remain (track %d)\n", 
		//	th[i].dt / old_tick_length, (th[i].trigger - curtime) / old_tick_length, i);
		//OutputDebugString(buf);
	}
}

void check_associations(void)
{
	DWORD disposition;
	HKEY key;
	unsigned long i;
	int goAway = 0;
	char buf[512];

	if (!alwaysCheckAssociations)
		return;

	// See if .mid files are registered to TMIDI
	RegCreateKeyEx(HKEY_CLASSES_ROOT, ".mid", 0, "", 
		REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, (unsigned long *) &disposition);
	if (key)
	{
		i = sizeof(buf);
		RegQueryValueEx(key, "", 0, NULL, (BYTE *) &buf, (unsigned long *) &i);
		RegCloseKey(key);
		if (strcmp(buf, "TMIDI"))
		{
			i = DialogBoxParam(ghInstance, MAKEINTRESOURCE(IDD_ASSOC), NULL, AssocDlg, (long) &goAway);
			if (goAway)
				alwaysCheckAssociations = FALSE;
			if (i)
				set_associations();
		}
		else
			set_associations();
	}
}

void set_associations(void)
{
	HKEY key, subkey, tmkey, shkey;
	char modulePath[512];
	DWORD disposition;
	char buf[512];

	// Get the program's filename
	GetModuleFileName(NULL, modulePath, sizeof(modulePath));

	// Set up registry entries to associate TMIDI with MIDI files
	// Registry entry for .mid
	RegCreateKeyEx(HKEY_CLASSES_ROOT, ".mid", 0, "", 
		REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, (unsigned long *) &disposition);
	strcpy(buf, "TMIDI");
	RegSetValueEx(key, "", 0, REG_SZ, (CONST BYTE *) buf, strlen(buf) + 1);
	RegCloseKey(key);
	// Registry key for TMIDI
	RegCreateKeyEx(HKEY_CLASSES_ROOT, "TMIDI", 0, "", 
		REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &tmkey, (unsigned long *) &disposition);
	strcpy(buf, "TMIDI File");
	RegSetValueEx(tmkey, "", 0, REG_SZ, (CONST BYTE *) buf, strlen(buf) + 1);
	// Registry key for TMIDI\DefaultIcon
	RegCreateKeyEx(tmkey, "DefaultIcon", 0, "", 
		REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, (unsigned long *) &disposition);
	strcpy(buf, modulePath);
	strcat(buf, ",0");
	RegSetValueEx(key, "", 0, REG_SZ, (CONST BYTE *) buf, strlen(buf) + 1);
	RegCloseKey(key);
	// Registry key for TMIDI\Shell
	RegCreateKeyEx(tmkey, "Shell", 0, "", 
		REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &shkey, (unsigned long *) &disposition);
	strcpy(buf, "Play");
	RegSetValueEx(shkey, "", 0, REG_SZ, (CONST BYTE *) buf, strlen(buf) + 1);
	// Registry key for TMIDI\Shell\open
	RegCreateKeyEx(shkey, "open", 0, "", 
		REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &subkey, (unsigned long *) &disposition);
	RegCreateKeyEx(subkey, "command", 0, "", 
		REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, (unsigned long *) &disposition);
	strcpy(buf, "\"");
	strcat(buf, modulePath);
	strcat(buf, "\" \"%1\"");
	RegSetValueEx(key, "", 0, REG_SZ, (CONST BYTE *) buf, strlen(buf) + 1);
	RegCloseKey(key);
	RegCloseKey(subkey);
	// Registry key for TMIDI\Shell\Play
	RegCreateKeyEx(shkey, "Play", 0, "", 
		REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &subkey, (unsigned long *) &disposition);
	strcpy(buf, "&Play in TMIDI");
	RegSetValueEx(subkey, "", 0, REG_SZ, (CONST BYTE *) buf, strlen(buf) + 1);
	RegCreateKeyEx(subkey, "command", 0, "", 
		REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, (unsigned long *) &disposition);
	strcpy(buf, "\"");
	strcat(buf, modulePath);
	strcat(buf, "\" \"%1\"");
	RegSetValueEx(key, "", 0, REG_SZ, (CONST BYTE *) buf, strlen(buf) + 1);
	RegCloseKey(key);
	RegCloseKey(subkey);
	RegCloseKey(shkey);
	RegCloseKey(tmkey);
}

// Dialog for file type association question
INT_PTR CALLBACK AssocDlg(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	static int *goAway = NULL;

	switch (iMsg)
	{
		case WM_INITDIALOG:
		{
			goAway = (int *) lParam;
			return TRUE;
		}
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDOK:
					*goAway = IsDlgButtonChecked(hDlg, IDC_GOAWAY) == BST_CHECKED;
					EndDialog(hDlg, TRUE);
					break;
				case IDCANCEL:
					PostMessage(hDlg, WM_CLOSE, 0, 0);
					break;
			}
			break;
		case WM_CLOSE:
		{
			*goAway = IsDlgButtonChecked(hDlg, IDC_GOAWAY) == BST_CHECKED;
			EndDialog(hDlg, FALSE);
			break;
		}
	}

	return FALSE;
}

playlist_t *playlist_add(char *filename)
{
	playlist_t *n, *p;

	n = (playlist_t *) malloc(sizeof(playlist_t));
	strcpy(n->filename, filename);
	n->prev = n->next = NULL;
	if (!playlist)
		playlist = n;
	else
	{
		for (p = playlist; p->next; p = p->next);
		p->next = n;
		n->prev = p;
	}

	return n;
}

void playlist_clear(void)
{
	playlist_t *p;

	while (playlist)
	{
		p = playlist->next;
		free(playlist);
		playlist = p;
	}
}

playlist_t *playlist_remove(playlist_t *target)
{
	return NULL;
}

// Gets a value from a scroll bar, to handle an HSCROLL or VSCROLL message
// The value is stored in i.
// The function returns 0 on success, 1 if no value was obtained.
int get_scroll_value(WPARAM wParam, LPARAM lParam, int *i)
{
	switch (LOWORD(wParam))
	{
		case TB_ENDTRACK:
			*i = SendMessage((HWND) lParam, TBM_GETPOS, 0, 0);
			return 0;
		case TB_THUMBPOSITION:
		case TB_THUMBTRACK:
			*i = HIWORD(wParam);
			return 0;
	}
	return 1;
}

void set_mod_pitch(signed int i)
{
	char buf[256];

	if (i != ms.mod_pitch)
		all_notes_off();
	ms.mod_pitch = i;
	SendDlgItemMessage(hwndApp, IDC_PITCH_SLIDER, TBM_SETPOS, TRUE, 24 + i);
	sprintf(buf, "Pitch: %d", i);
	SetDlgItemText(hwndApp, IDC_PITCH, buf);
}

void set_mod_velocity(signed int i)
{
	char buf[256];

	ms.mod_velocity = i;
	SendDlgItemMessage(hwndApp, IDC_VELOCITY_SLIDER, TBM_SETPOS, TRUE, 64 + i);
	sprintf(buf, "Velocity: %d", i);
	SetDlgItemText(hwndApp, IDC_VELOCITY, buf);
}

void set_channel_mute(int channel, int mute)
{
	unsigned char vel, note, was_muted;
	HWND hwnd = GetDlgItem(hwndApp, IDC_C0 + channel);

//	style = GetWindowLong(hwnd, GWL_STYLE);

	was_muted = ms.channels[channel].muted;
	ms.channels[channel].muted = mute;

	if (mute)
	{
		// Mute the channel
		all_notes_off_channel(channel);
//		style |= SS_SUNKEN;
	}
	else
	{
		// Unmute the channel

		// Restore any notes that should be playing (unless this is the percussion channel)
		if (channel != 9 && was_muted)
			for (note = 0; note < 128; note++)
			{
				vel = ms.channels[channel].notes[note];
				if (vel)
					note_on(TRUE, note, vel, channel);
			}
		// Change the channel text
//		style &= ~SS_SUNKEN;
	}

	SendMessage(hwnd, BM_SETCHECK, (WPARAM) mute, 0);
	//SetWindowLong(hwnd, GWL_STYLE, style);
	//SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_DRAWFRAME);
	//InvalidateRect(hwnd, NULL, TRUE);
}

void set_channel_solo(int channel)
{
	int i, already_soloed;

	already_soloed = 1;
	for (i = 0; i < 16; i++)
		if (!ms.channels[i].muted && i != channel)
			already_soloed = 0;

	if (already_soloed)
		for (i = 0; i < 16; i++)
			set_channel_mute(i, FALSE);
	else
		for (i = 0; i < 16; i++)
			set_channel_mute(i, i != channel);
}

// Wndproc for subclassing buttons/checkboxes, to catch right mouse clicks
LRESULT CALLBACK NewButtonProc(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	if (iMsg == WM_RBUTTONUP)
		PostMessage(GetParent(hDlg), WM_COMMAND, MAKELONG(GetDlgCtrlID(hDlg), WM_RBUTTONUP), (LPARAM) hDlg);

	return CallWindowProc(OldButtonProc, hDlg, iMsg, wParam, lParam);
}

// Dialog for MIDI tracks window
INT_PTR CALLBACK TracksDlg(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	static HWND hwndLV;

	switch (iMsg)
	{
		case WM_INITDIALOG:
		{
			hwndTracks = hDlg;
			//SetForegroundWindow(hDlg);
			if (tracksRectSaved)
				SetWindowPos(hDlg, 0, tracksRect.left, tracksRect.top, tracksRect.right - tracksRect.left, tracksRect.bottom - tracksRect.top, SWP_NOZORDER);

			hwndLV = GetDlgItem(hDlg, IDC_TRACKS_LIST);
			InitTracksListView(hwndLV);
			FillTracksListView(hwndLV);

			return TRUE;
		}
		case WM_SIZE:
		{
			MoveWindow(GetDlgItem(hDlg, IDC_TRACKS_LIST), 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
			break;
		}
		case WM_CLOSE:
		{
			GetWindowRect(hDlg, &tracksRect);
			tracksRectSaved = 1;
			hwndTracks = 0;
			DestroyWindow(hDlg);
			break;
		}
	}

	return FALSE;
}

#define NUM_TRACK_COLUMNS	12
void InitTracksListView(HWND hwndLV)
{
	LV_COLUMN lvc;
	char *columns[NUM_TRACK_COLUMNS] = { 
		"Track", 
		"Bytes", 
		"Event", 
		"Events", 
		"Channel", 
		"Name", 
		"Program", 
		"Bank", 
		"Note", 
		"Last Controller", 
		"Value", 
		"Pitch Bend"
	};
	int column_width[NUM_TRACK_COLUMNS] = { 
		40,			// Track
		45,			// Bytes
		45,			// Event
		45,			// Events
		52,			// Channel
		150,		// Name
		120,		// Program
		37,			// Bank
		35,			// Note
		110,		// Last Controller
		39,			// Value
		65			// Pitch bend
	};
	int i;
	DWORD dwStyle;

	dwStyle = ListView_GetExtendedListViewStyle(hwndLV);
	dwStyle |= LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP;
	ListView_SetExtendedListViewStyle(hwndLV, dwStyle);

	// Initialize the LV_COLUMN structure. 
	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT; // | LVCF_SUBITEM;

	// Insert the columns
	for (i = 0; i < NUM_TRACK_COLUMNS; i++)
	{
		lvc.cx = column_width[i];
		lvc.pszText = columns[i];
		if (i == 5 || i == 6 || i == 9)
			lvc.fmt = LVCFMT_LEFT;
		else
			lvc.fmt = LVCFMT_RIGHT;
		ListView_InsertColumn(hwndLV, i, &lvc);
	}

	// Insert the track items
	SetupItemsTracksListView(hwndLV);
}

// Insert the track items
void SetupItemsTracksListView(HWND hwndLV)
{
	LVITEM lvi;
	int iItem;

	// Reset the list view by deleting everything in it
	ListView_DeleteAllItems(hwndLV);

	// Clear the cache flag used in FillTracksListView()
	tracksLastValuesSet = 0;

    // Initialize LV_ITEM members that are common to all items.
	lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM | LVIF_STATE;
	lvi.state = 0;
	lvi.stateMask = 0; 
	lvi.iImage = 0;                     // image list index
	lvi.lParam = 0;						// item data

	for (iItem = 0; iItem < mh.num_tracks; iItem++)
	{
        // Initialize item-specific LV_ITEM members.
		lvi.iItem = iItem;

		// Item, message ID #
		lvi.iSubItem = 0;
		lvi.pszText = "";
		ListView_InsertItem(hwndLV, &lvi);
	}
}

// Sets an integer in the tracks list view control, IF its contents have changed
void SetCachedLVItem(HWND hwndLV, int iItem, int column, signed int &lastval, int val)
{
	char buf[64];

	if (val != lastval)
	{
		lastval = val;
		itoa(val, buf, 10);
		ListView_SetItemText(hwndLV, iItem, column, buf);
	}
}

// Sets a string in the tracks list view control, IF its contents have changed
void SetCachedLVItem(HWND hwndLV, int iItem, int column, signed int &lastval, char *str)
{
	signed int h;
	char *ch = str;

	// Do a simple hash of the string to form the lastval
	h = 0;
	while (*ch)
		h += *ch++;
	if (h != lastval)
	{
		lastval = h;
		ListView_SetItemText(hwndLV, iItem, column, str);
	}
}

// Updates the tracks list view control
void FillTracksListView(HWND hwndLV)
{
	int iItem, i, j;
	static signed int last_value[NUM_TRACK_COLUMNS][MAX_MIDI_TRACKS];

	if (!tracksLastValuesSet)
	{
		tracksLastValuesSet = 1;
		for (i = 0; i < NUM_TRACK_COLUMNS; i++)
			for (j = 0; j < MAX_MIDI_TRACKS; j++)
				last_value[i][j] = -1;
	}

	for (iItem = 0; iItem < mh.num_tracks; iItem++)
	{
		// 0: Track number
		SetCachedLVItem(hwndLV, iItem, 0, last_value[0][iItem], (int) (th[iItem].tracknum + 1));
		// 1: Track length (in bytes)
		SetCachedLVItem(hwndLV, iItem, 1, last_value[1][iItem], th[iItem].length);
		// 2: Number of events done
		SetCachedLVItem(hwndLV, iItem, 2, last_value[2][iItem], th[iItem].num_events_done);
		// 3: Total number of events
		SetCachedLVItem(hwndLV, iItem, 3, last_value[3][iItem], th[iItem].num_events);
		// 4: Track channel (from last event)
		if (th[iItem].last_channel != -1)
			SetCachedLVItem(hwndLV, iItem, 4, last_value[4][iItem], th[iItem].last_channel + 1);
		// 5: Track name
		SetCachedLVItem(hwndLV, iItem, 5, last_value[5][iItem], th[iItem].name);
		// 6: Last program selected by this track
		if (th[iItem].last_program != -1)
		{
			if (th[iItem].last_channel != 9)
				SetCachedLVItem(hwndLV, iItem, 6, last_value[6][iItem], get_program_name(th[iItem].last_program, 0));
			else
				SetCachedLVItem(hwndLV, iItem, 6, last_value[6][iItem], get_drum_kit_name(th[iItem].last_program));
		}
		// 7: Last bank selected by this track
		if (th[iItem].last_bank != -1)
			SetCachedLVItem(hwndLV, iItem, 7, last_value[7][iItem], th[iItem].last_bank);
		// 8: Last note from this track
		if (th[iItem].last_note_pitch != -1)
			SetCachedLVItem(hwndLV, iItem, 8, last_value[8][iItem], th[iItem].last_note_pitch);
		// 9: Last controller from this track
		if (th[iItem].last_controller != -1)
			SetCachedLVItem(hwndLV, iItem, 9, last_value[9][iItem], controller_names[th[iItem].last_controller]);
		// 10: Last controller value from this track
		if (th[iItem].last_controller_value != -1)
			SetCachedLVItem(hwndLV, iItem, 10, last_value[10][iItem], th[iItem].last_controller_value);
		// 11: Last pitch bend from this track
		if (th[iItem].last_pitch_bend != -1)
			SetCachedLVItem(hwndLV, iItem, 11, last_value[11][iItem], th[iItem].last_pitch_bend);
	}
}

//////////////////// CHANNELS WINDOW BEGIN
// Dialog for MIDI tracks window
INT_PTR CALLBACK ChannelsDlg(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	static HWND hwndLV;

	switch (iMsg)
	{
		case WM_INITDIALOG:
		{
			hwndChannels = hDlg;
			//SetForegroundWindow(hDlg);
			if (channelsRectSaved)
				SetWindowPos(hDlg, 0, channelsRect.left, channelsRect.top, channelsRect.right - channelsRect.left, channelsRect.bottom - channelsRect.top, SWP_NOZORDER);

			hwndLV = GetDlgItem(hDlg, IDC_CHANNELS_LIST);
			InitChannelsListView(hwndLV);
			FillChannelsListView(hwndLV);

			return TRUE;
		}
		case WMAPP_REFRESH_CHANNELS:
			FillChannelsListView(hwndLV);
			break;
		case WM_SIZE:
		{
			MoveWindow(GetDlgItem(hDlg, IDC_CHANNELS_LIST), 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
			break;
		}
		case WM_CLOSE:
		{
			GetWindowRect(hDlg, &channelsRect);
			channelsRectSaved = 1;
			hwndChannels = 0;
			DestroyWindow(hDlg);
			break;
		}
	}

	return FALSE;
}

#define NUM_CHANNEL_COLUMNS	7
void InitChannelsListView(HWND hwndLV)
{
	LV_COLUMN lvc;
	char *columns[NUM_CHANNEL_COLUMNS] = { 
		"Channel", 
		"Program", 
		"Bank", 
		"Note", 
		"Last Controller", 
		"Value", 
		"Pitch Bend"
	};
	int column_width[NUM_CHANNEL_COLUMNS] = { 
		52,			// Channel
		120,		// Program
		37,			// Bank
		35,			// Note
		140,		// Last Controller
		39,			// Value
		65			// Pitch bend
	};
	int i;
	DWORD dwStyle;

	dwStyle = ListView_GetExtendedListViewStyle(hwndLV);
	dwStyle |= LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP;
	ListView_SetExtendedListViewStyle(hwndLV, dwStyle);

	// Initialize the LV_COLUMN structure. 
	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT; // | LVCF_SUBITEM;

	// Insert the columns
	for (i = 0; i < NUM_CHANNEL_COLUMNS; i++)
	{
		lvc.cx = column_width[i];
		lvc.pszText = columns[i];
		if (i == 1 || i == 4)
			lvc.fmt = LVCFMT_LEFT;
		else
			lvc.fmt = LVCFMT_RIGHT;
		ListView_InsertColumn(hwndLV, i, &lvc);
	}

	// Insert the channel items
	SetupItemsChannelsListView(hwndLV);
}

// Insert the channel items
void SetupItemsChannelsListView(HWND hwndLV)
{
	LVITEM lvi;
	int iItem;
	char buf[16];

	// Reset the list view by deleting everything in it
	ListView_DeleteAllItems(hwndLV);

	// Clear the cache flag used in FillChannelsListView()
	channelsLastValuesSet = 0;

    // Initialize LV_ITEM members that are common to all items.
	lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM | LVIF_STATE;
	lvi.state = 0;
	lvi.stateMask = 0; 
	lvi.iImage = 0;                     // image list index
	lvi.lParam = 0;						// item data

	for (iItem = 0; iItem < 16; iItem++)
	{
        // Initialize item-specific LV_ITEM members.
		lvi.iItem = iItem;

		// Item, message ID #
		lvi.iSubItem = 0;
		itoa(iItem + 1, buf, 10);
		lvi.pszText = buf;
		ListView_InsertItem(hwndLV, &lvi);
	}
}

// Updates the channels list view control
void FillChannelsListView(HWND hwndLV)
{
	int iItem, i, j;
	static signed int last_value[NUM_CHANNEL_COLUMNS][16];

	if (!channelsLastValuesSet)
	{
		channelsLastValuesSet = 1;
		for (i = 0; i < NUM_CHANNEL_COLUMNS; i++)
			for (j = 0; j < 16; j++)
				last_value[i][j] = -1;
	}

	for (iItem = 0; iItem < 16; iItem++)
	{
		// 0: Channel number
		SetCachedLVItem(hwndLV, iItem, 0, last_value[0][iItem], iItem + 1);
		// 1: Last program selected for this channel
		if (ms.channels[iItem].last_program != -1)
		{
			if (iItem != 9)
				SetCachedLVItem(hwndLV, iItem, 1, last_value[1][iItem], get_program_name(ms.channels[iItem].last_program, 0));
			else
				SetCachedLVItem(hwndLV, iItem, 1, last_value[1][iItem], get_drum_kit_name(ms.channels[iItem].last_program));
		}
		// 2: Last bank selected by this channel
		if (ms.channels[iItem].last_bank != -1)
			SetCachedLVItem(hwndLV, iItem, 2, last_value[2][iItem], ms.channels[iItem].last_bank);
		// 3: Last note from this channel
		if (ms.channels[iItem].last_note_pitch != -1)
			SetCachedLVItem(hwndLV, iItem, 3, last_value[3][iItem], ms.channels[iItem].last_note_pitch);
		// 4: Last controller from this channel
		if (ms.channels[iItem].last_controller != -1)
			SetCachedLVItem(hwndLV, iItem, 4, last_value[4][iItem], controller_names[ms.channels[iItem].last_controller]);
		// 5: Last controller value from this channel
		if (ms.channels[iItem].last_controller_value != -1)
			SetCachedLVItem(hwndLV, iItem, 5, last_value[5][iItem], ms.channels[iItem].last_controller_value);
		// 6: Last pitch bend from this channel
		if (ms.channels[iItem].last_pitch_bend != -1)
			SetCachedLVItem(hwndLV, iItem, 6, last_value[6][iItem], ms.channels[iItem].last_pitch_bend);
	}
}
//////////////////// CHANNELS WINDOW END

// Dialog for MIDI sysex window
INT_PTR CALLBACK SysexDlg(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	static HWND hwndLV;
	LPNMHDR nmhdr;
	LVITEM lvi;
	HANDLE hClipdata;
	char *clipdata;
	int i;
	unsigned char ch, hi, lo;
	midi_sysex_t *s;
	HWND gendlg;
	unsigned char *genbuf, *genptr;

	switch (iMsg)
	{
		case WM_INITDIALOG:
			hwndSysex = hDlg;
			//SetForegroundWindow(hDlg);
			if (sysexRectSaved)
				SetWindowPos(hDlg, 0, sysexRect.left, sysexRect.top, sysexRect.right - sysexRect.left, sysexRect.bottom - sysexRect.top, SWP_NOZORDER);

			hwndLV = GetDlgItem(hDlg, IDC_SYSEX_LIST);
			InitSysexListView(hwndLV);

			return TRUE;

		case WM_SIZE:
			MoveWindow(GetDlgItem(hDlg, IDC_SYSEX_LIST), 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
			break;

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDCANCEL:
					PostMessage(hDlg, WM_CLOSE, 0, 0);
					return TRUE;
			}
			break;

		case WM_CLOSE:
			GetWindowRect(hDlg, &sysexRect);
			sysexRectSaved = 1;
			hwndSysex = 0;
			DestroyWindow(hDlg);
			break;

		case WM_NOTIFY:
			nmhdr = (LPNMHDR) lParam;
			switch (nmhdr->idFrom)
			{
				case IDC_SYSEX_LIST:
					switch (nmhdr->code)
					{
						case NM_RCLICK:
							// Get the selected item
							i = ListView_GetNextItem(hwndLV, -1, LVNI_SELECTED);
							if (i == -1)
								break;
							// Get the sysex pointer from the list item's LPARAM
							memset(&lvi, 0, sizeof(lvi));
							lvi.iItem = i;
							lvi.mask = LVIF_PARAM;
							if (!ListView_GetItem(hwndLV, &lvi))
								break;
							s = (midi_sysex_t *) lvi.lParam;
							// Open a generic text window
							gendlg = CreateDialog(ghInstance, MAKEINTRESOURCE(IDD_GENERIC_TEXT), hDlg, (DLGPROC) GenericTextDlg);
							// Allocate a temp buffer to hold the sysex data in text format
							genptr = genbuf = (unsigned char *) malloc(s->length * 4);
							// Fill the buffer
							i = 0;
							while (i < s->length)
							{
								ch = s->data[i++];
								if (isprint(ch))
									*genptr++ = ch;
								else
									*genptr++ = '.';
								// Hex view
								/*hi = HINYBBLE(ch);
								lo = LONYBBLE(ch);
								*genptr++ = (hi > 9) ? (hi - 10 + 'A') : (hi + '0');
								*genptr++ = (lo > 9) ? (lo - 10 + 'A') : (lo + '0');
								*genptr++ = ' ';*/
								if (i % 16 == 0)
								{
									*genptr++ = '\r';
									*genptr++ = '\n';
								}
							}
							*genptr = '\0';
							// Put this text into the generic text window
							SetDlgItemText(gendlg, IDC_GENERIC_TEXT, (char *) genbuf);
							// Free the temp buffer
							free(genbuf);
							break;
						case NM_DBLCLK:
							// Get the selected item
							i = ListView_GetNextItem(hwndLV, -1, LVNI_SELECTED);
							if (i == -1)
								break;
							// Get the sysex pointer from the list item's LPARAM
							memset(&lvi, 0, sizeof(lvi));
							lvi.iItem = i;
							lvi.mask = LVIF_PARAM;
							if (!ListView_GetItem(hwndLV, &lvi))
								break;
							s = (midi_sysex_t *) lvi.lParam;
							// Allocate enough memory to store the sysex data in text format
							hClipdata = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, s->length * 3 + 3);
							// Lock the memory
							clipdata = (char *) GlobalLock(hClipdata);
							if (clipdata)
							{
								// Fill it with the sysex data in text format
								clipdata[0] = 'F';
								clipdata[1] = '0';
								clipdata[2] = ' ';
								for (i = 0; i < s->length; i++)
								{
									ch = s->data[i];
									hi = HINYBBLE(ch);
									lo = LONYBBLE(ch);
									clipdata[3 + i * 3]     = (hi > 9) ? (hi - 10 + 'A') : (hi + '0');
									clipdata[3 + i * 3 + 1] = (lo > 9) ? (lo - 10 + 'A') : (lo + '0');
									clipdata[3 + i * 3 + 2] = ' ';
								}
								clipdata[3 + i * 3 - 1] = '\0';
								// Unlock it
								GlobalUnlock(hClipdata);
								// Copy the sysex text to the clipboard
								if (OpenClipboard(hwndApp))
								{
									EmptyClipboard();
									SetClipboardData(CF_TEXT, hClipdata);
									CloseClipboard();
								}
							}
							break;
					}
					break;
			}
			break;
	}

	return FALSE;
}

#define NUM_SYSEX_COLUMNS	11
void InitSysexListView(HWND hwndLV)
{
	LV_COLUMN lvc;
	char *columns[NUM_TRACK_COLUMNS] = { 
		"Time", 
		"Track", 
		"Offset", 
		"Ch.", 
		"Length", 
		"Data", 
		"Manufacturer", 
		"Device", 
		"Model", 
		"Cmd", 
		"Interpretation"
	};
	int column_width[NUM_TRACK_COLUMNS] = { 
		40,			// Time
		40,			// Track
		48,			// Track Offset
		30,			// Channel
		45,			// Length
		200,		// Data
		84,			// Manufacturer
		46,			// Device
		52,			// Model
		35,			// Command
		190			// Interpretation
	};
	int i;
	DWORD dwStyle;

	dwStyle = ListView_GetExtendedListViewStyle(hwndLV);
	dwStyle |= LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP;
	ListView_SetExtendedListViewStyle(hwndLV, dwStyle);

	// Initialize the LV_COLUMN structure. 
	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT; // | LVCF_SUBITEM;

	// Insert the columns
	for (i = 0; i < NUM_SYSEX_COLUMNS; i++)
	{
		lvc.cx = column_width[i];
		lvc.pszText = columns[i];
		if (i == 0 || i >= 5)
			lvc.fmt = LVCFMT_LEFT;
		else
			lvc.fmt = LVCFMT_RIGHT;
		ListView_InsertColumn(hwndLV, i, &lvc);
	}

	// Insert the track items
	SetupItemsSysexListView(hwndLV);
}

// Insert the sysex items and set their values
void SetupItemsSysexListView(HWND hwndLV)
{
	LVITEM lvi;
	int iItem;
	midi_sysex_t *p;
	char buf[2048];
	unsigned char ch, hi, lo;
	int i, j;
	unsigned char *s;

	// Reset the list view by deleting everything in it
	ListView_DeleteAllItems(hwndLV);

    // Initialize LV_ITEM members that are common to all items.
	lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM | LVIF_STATE;
	lvi.state = 0;
	lvi.stateMask = 0; 
	lvi.iImage = 0;                     // image list index
	lvi.lParam = 0;						// item data

	iItem = 0;
	for (p = midi_sysex_events; p; p = p->next)
	{
        // Initialize item-specific LV_ITEM members.
		lvi.iItem = iItem;

		// Item, message ID #
		lvi.iSubItem = 0;
		lvi.lParam = (LPARAM) p;
		lvi.pszText = "";
		ListView_InsertItem(hwndLV, &lvi);

		// Set the various items for this sysex event
		sprintf(buf, "%d:%02d", ((int) p->midi_time) / 60, ((int) p->midi_time) % 60);
		ListView_SetItemText(hwndLV, iItem, 0, buf);
		itoa(p->track, buf, 10);
		ListView_SetItemText(hwndLV, iItem, 1, buf);
		itoa(p->track_offset, buf, 10);
		ListView_SetItemText(hwndLV, iItem, 2, buf);
		itoa(p->channel, buf, 10);
		ListView_SetItemText(hwndLV, iItem, 3, buf);
		itoa(p->length, buf, 10);
		ListView_SetItemText(hwndLV, iItem, 4, buf);
		// 5: Sysex data
		s = p->data;
		j = (p->length > 64) ? 64 : p->length;
		// Render data bytes
		buf[0] = 'F';
		buf[1] = '0';
		buf[2] = ' ';
		for (i = 0; i < j; i++)
		{
			ch = p->data[i];
			hi = HINYBBLE(ch);
			lo = LONYBBLE(ch);
			buf[3 + i * 3]     = (hi > 9) ? (hi - 10 + 'A') : (hi + '0');
			buf[3 + i * 3 + 1] = (lo > 9) ? (lo - 10 + 'A') : (lo + '0');
			buf[3 + i * 3 + 2] = ' ';
		}
		buf[3 + i * 3] = '\0';
		ListView_SetItemText(hwndLV, iItem, 5, buf);
		// 6: Manufacturer
		sprintf(buf, "%s", get_sysex_manufacturer_name(s[0]));
		ListView_SetItemText(hwndLV, iItem, 6, buf);
		// 7: Device
		if (s[1] == 127)
			strcpy(buf, "(All)");
		else
			sprintf(buf, "0x%02X", (unsigned int) s[1]);
		ListView_SetItemText(hwndLV, iItem, 7, buf);
		// 8: Model
		strcpy(buf, interpret_model(s));
		ListView_SetItemText(hwndLV, iItem, 8, buf);
		// 9: Command
		strcpy(buf, interpret_command(s));
		ListView_SetItemText(hwndLV, iItem, 9, buf);
		// 10: Interpretation
		strcpy(buf, interpret_sysex(s, p->length));
		ListView_SetItemText(hwndLV, iItem, 10, buf);

		iItem++;
	}
}

void output_sysex_data(unsigned char channel, unsigned char *data, int length)
{
	MIDIHDR mh;
	static unsigned char *buf = (unsigned char *) malloc(256);
	int buflen = 256;

	if (!hout)
		return;

	// Realloc the sysex buffer if it's too small
	if (length + 2 > buflen)
	{
		buflen = length + 32;
		buf = (unsigned char *) realloc(buf, buflen);
	}

	buf[0] = 0xF0;					// Sysex begin command
	memcpy(buf + 1, data, length);	// Copy the sysex data into the local sysex buffer
	
	// Prepare the MIDI out header
	memset(&mh, 0, sizeof(mh));
	mh.lpData = (char *) buf;
	mh.dwBufferLength = length + 1;
	mh.dwBytesRecorded = length + 1;
	// Prepare the sysex buffer for output
	midiOutPrepareHeader(hout, &mh, sizeof(mh));

	// Send the sysex buffer!
	midiOutLongMsg(hout, &mh, sizeof(mh));

	// Unprepare the sysex buffer
	midiOutUnprepareHeader(hout, &mh, sizeof(mh));
}

// Interprets a sysex string
char *interpret_sysex(unsigned char *s, int len)
{
	static char buf[256], timbreName[16];
	int i, j;

	buf[0] = '\0';

	switch (s[0])								// Manufacturer ID
	{
		case 0x41:	// Roland
			switch (s[2])						// Model ID
			{
				case 0x16:	// MT-32
					switch (s[4])
					{
						case 0x03:	// Patch Temp Area
							sprintf(buf, "Patch Temp Area, Part %d", HINYBBLE(s[6]) + 1);
							break;
						case 0x04:	// Timbre Temp Area
							strcpy(buf, "Timbre Temp Area");
							break;
						case 0x05:	// Patch Memory
							strcpy(buf, "Patch Memory");
							interpret_mt32_patch_memory(s, len);
							break;
						case 0x08:	// Timbre Memory
							strcpy(buf, "Timbre Memory: ");
							interpret_mt32_timbre_memory(s, len);
							strncat(buf, (char *) &s[7], 10);
							break;
						case 0x10:	// System Area
							strcpy(buf, "System Area");
							break;
						case 0x20:	// Display Area
							strcpy(buf, "MT-32 Display: ");
							j = strlen(buf);
							for (i = 7; i < len - 2; i++)
								buf[j++] = s[i];
							buf[j] = '\0';
							break;
					}
					break;
				case 0x42:	// GS Message
					switch (s[4])
					{
						case 0x00:	// SC-88 System Parameter
							switch (s[5])
							{
								case 0x00:	// System message
									switch (s[6])
									{
										case 0x7F:
											//sprintf(buf, "SC-88 System Mode Set: %s", s[7] ? "Double Module Mode" : "Single Module Mode");
											sprintf(buf, "SC-88 Reset (%s)", s[7] ? "Double" : "Single");
											break;
									}
									break;
								case 0x01:	// Part message
									sprintf(buf, "SC-88 Part %c%d: Port %c", HINYBBLE(s[6]) + 'A', interpret_sysex_part(LONYBBLE(s[6])), s[7] + 'A');
									break;
							}
							break;
						case 0x08:	// SC-88 Bulk Dump Setup Parameters
							strcpy(buf, "SC-88 Bulk Dump Setup Parameters");
							break;
						case 0x40:	// Parameter byte
							if (s[5] >= 0x10 && s[5] <= 0x1F)
							{
								sprintf(buf, "Part %d: ", interpret_sysex_part(s[5]));
								switch (s[6])
								{
									case 0x00:
										sprintf(&buf[strlen(buf)], "Tone #: Bank %d, Patch %d", s[7], s[8]);
										break;
									case 0x01:
										sprintf(&buf[strlen(buf)], "Rx Control Change: %s", s[7] ? "On" : "Off");
										break;
									case 0x02:
										sprintf(&buf[strlen(buf)], "Rx Channel: %d", s[7]);
										break;
									case 0x03:
										sprintf(&buf[strlen(buf)], "Rx Pitch Bend: %s", s[7] ? "On" : "Off");
										break;
									case 0x04:
										sprintf(&buf[strlen(buf)], "Rx Channel Pressure: %s", s[7] ? "On" : "Off");
										break;
									case 0x05:
										sprintf(&buf[strlen(buf)], "Rx Program Change: %s", s[7] ? "On" : "Off");
										break;
									case 0x06:
										sprintf(&buf[strlen(buf)], "Rx Control Change: %s", s[7] ? "On" : "Off");
										break;
									case 0x07:
										sprintf(&buf[strlen(buf)], "Rx Poly Pressure: %s", s[7] ? "On" : "Off");
										break;
									case 0x08:
										sprintf(&buf[strlen(buf)], "Rx Note Message: %s", s[7] ? "On" : "Off");
										break;
									case 0x09:
										sprintf(&buf[strlen(buf)], "RPN: %s", s[7] ? "On" : "Off");
										break;
									case 0x0A:
										sprintf(&buf[strlen(buf)], "NRPN: %s", s[7] ? "On" : "Off");
										break;
									case 0x0B:
										sprintf(&buf[strlen(buf)], "Rx Modulation: %s", s[7] ? "On" : "Off");
										break;
									case 0x0C:
										sprintf(&buf[strlen(buf)], "Rx Volume: %s", s[7] ? "On" : "Off");
										break;
									case 0x0D:
										sprintf(&buf[strlen(buf)], "Rx Panpot: %s", s[7] ? "On" : "Off");
										break;
									case 0x0E:
										sprintf(&buf[strlen(buf)], "Rx Expression: %s", s[7] ? "On" : "Off");
										break;
									case 0x0F:
										sprintf(&buf[strlen(buf)], "Rx Hold 1: %s", s[7] ? "On" : "Off");
										break;
									case 0x10:
										sprintf(&buf[strlen(buf)], "Rx Portamento: %s", s[7] ? "On" : "Off");
										break;
									case 0x11:
										sprintf(&buf[strlen(buf)], "Rx Sostenuto: %s", s[7] ? "On" : "Off");
										break;
									case 0x12:
										sprintf(&buf[strlen(buf)], "Rx Soft Pedal: %s", s[7] ? "On" : "Off");
										break;
									case 0x13:
										sprintf(&buf[strlen(buf)], "Mono/Poly Mode: %s", s[7] ? "On" : "Off");
										break;
									case 0x14:
										sprintf(&buf[strlen(buf)], "Assign Mode: %s", (s[7] > 0) ? ((s[7] == 1) ? "Limited" : "Full-Multi") : "Single");
										break;
									case 0x15:
										sprintf(&buf[strlen(buf)], "Use for Rhythm Part: %s", (s[7] > 0) ? ((s[7] == 1) ? "Map 1" : "Map 2") : "Off");
										break;
									case 0x16:
										sprintf(&buf[strlen(buf)], "Pitch Key Shift: %d", s[7]);
										break;
									case 0x17:
										sprintf(&buf[strlen(buf)], "Pitch Offset Fine: %d", s[7]);
										break;
									case 0x19:
										sprintf(&buf[strlen(buf)], "Rx Part Level: %d", s[7]);
										break;
									case 0x1A:
										sprintf(&buf[strlen(buf)], "Rx Velocity Sense Depth: %d", s[7]);
										break;
									case 0x1B:
										sprintf(&buf[strlen(buf)], "Rx Velocity Sense Offset: %d", s[7]);
										break;
									case 0x1C:
										sprintf(&buf[strlen(buf)], "Rx Part Panpot: %d", s[7]);
										break;
									case 0x1D:
										sprintf(&buf[strlen(buf)], "Key Range Low: %d", s[7]);
										break;
									case 0x1E:
										sprintf(&buf[strlen(buf)], "Key Range High: %d", s[7]);
										break;
									case 0x1F:
										sprintf(&buf[strlen(buf)], "CC1 Controller Number: %d", s[7]);
										break;
									case 0x20:
										sprintf(&buf[strlen(buf)], "CC2 Controller Number: %d", s[7]);
										break;
									case 0x21:
										sprintf(&buf[strlen(buf)], "Chorus Send Level: %d", s[7]);
										break;
									case 0x22:
										sprintf(&buf[strlen(buf)], "Reverb Send Level: %d", s[7]);
										break;
									case 0x23:
										sprintf(&buf[strlen(buf)], "Rx Bank Select: %s", s[7] ? "On" : "Off");
										break;
									case 0x24:
										sprintf(&buf[strlen(buf)], "SC-88 Rx Bank Select LSB: %s", s[7] ? "On" : "Off");
										break;
									case 0x2A:
										sprintf(&buf[strlen(buf)], "SC-88 Pitch Tune Fine: %d", s[7]);
										break;
									case 0x30:
										sprintf(&buf[strlen(buf)], "Vibrato Rate: %d", s[7]);
										break;
									case 0x31:
										sprintf(&buf[strlen(buf)], "Vibrato Depth: %d", s[7]);
										break;
									case 0x32:
										sprintf(&buf[strlen(buf)], "TVF Cutoff Frequency: %d", s[7] - 64);
										break;
									case 0x33:
										sprintf(&buf[strlen(buf)], "TVF Resonance: %d", s[7] - 64);
										break;
									case 0x34:
										sprintf(&buf[strlen(buf)], "TVF/TVA Envelope Attack: %d", s[7] - 64);
										break;
									case 0x35:
										sprintf(&buf[strlen(buf)], "TVF/TVA Envelope Decay: %d", s[7] - 64);
										break;
									case 0x36:
										sprintf(&buf[strlen(buf)], "TVF/TVA Envelope Release: %d", s[7] - 64);
										break;
									case 0x37:
										sprintf(&buf[strlen(buf)], "Vibrato Delay: %d", s[7]);
										break;
									case 0x40:
										sprintf(&buf[strlen(buf)], "Scale Tuning");
										break;
								}
							}
							else
							if (s[5] >= 0x20 && s[5] <= 0x2F)
							{
								strcpy(buf, "VCF/VCO/VCA Parameter");
							}
							else
							if (s[5] >= 0x40 && s[5] <= 0x4F)
							{
								sprintf(buf, "SC-88 Part %d: ", interpret_sysex_part(s[5]));
SC88PartParam:
								switch (s[6])
								{
									case 0x13:
										sprintf(&buf[strlen(buf)], "Mono/Poly mode: %s", s[7] ? "Polyphonic" : "Mono");
										break;
									case 0x20:
										sprintf(&buf[strlen(buf)], "EQ: %s", s[7] ? "On" : "Off");
										break;
									case 0x21:
										strcat(buf, "Output Assign: ");
										switch (s[7])
										{
											case 0x00: strcat(buf, "Output 1"); break;
											case 0x01: strcat(buf, "Output 2"); break;
											case 0x02: strcat(buf, "Output 2-L"); break;
											case 0x03: strcat(buf, "Output 2-R"); break;
											default: strcat(buf, itoa(s[7], buf, 10));
										}
										break;
									case 0x22:
										sprintf(&buf[strlen(buf)], "EFX: %s", s[7] ? "On" : "Off");
										break;
									case 0x2A:
										sprintf(&buf[strlen(buf)], "SC-88 Pitch Tune Fine: %d", s[7]);
										break;
								}
							}
							else
							switch (s[5])
							{
								case 0x00:	// System parameter
									switch (s[6])
									{
										case 0x00:	// Master Tune
										case 0x02:
										case 0x03:
											strcpy(buf, "Master Tune");
											break;
										case 0x04:	// Master Volume
											strcpy(buf, "Master Volume");
											break;
										case 0x05:	// Master Key Shift
											strcpy(buf, "Master Key Shift");
											break;
										case 0x06:	// Master Pan
											strcpy(buf, "Master Pan");
											break;
										case 0x7F:	// GS Reset
											strcpy(buf, "GS Reset");	// F0 41 10 42 12 40 00 7F 00 41 F7
											break;
									}
									break;
								case 0x01:	// Patch parameter
									switch (s[6])
									{
										case 0x00:	// Patch Name
											strcpy(buf, "Patch Name: ");
											j = strlen(buf);
											for (i = 8; i < len - 2; i++)
												buf[j++] = s[i];
											buf[j] = '\0';							
											break;
										case 0x10:	// Voice Reserve
											strcpy(buf, "Voice Reserve");
											break;
										case 0x30:	// Reverb Macro
											strcpy(buf, "Reverb Macro: ");
											switch (s[7])
											{
												case 0x00: strcat(buf, "Room 1"); break;
												case 0x01: strcat(buf, "Room 2"); break;
												case 0x02: strcat(buf, "Room 3"); break;
												case 0x03: strcat(buf, "Hall 1"); break;
												case 0x04: strcat(buf, "Hall 2"); break;
												case 0x05: strcat(buf, "Plate"); break;
												case 0x06: strcat(buf, "Delay"); break;
												case 0x07: strcat(buf, "Panning Delay"); break;
											}
											break;
										case 0x31:	// Reverb Character
											sprintf(buf, "Reverb Character: %d", s[7]);
											break;
										case 0x32:	// Reverb PRE-LPF
											sprintf(buf, "Reverb PRE-LPF: %d", s[7]);
											break;
										case 0x33:	// Reverb Level
											sprintf(buf, "Reverb Level: %d", s[7]);
											break;
										case 0x34:	// Reverb Time
											sprintf(buf, "Reverb Time: %d", s[7]);
											break;
										case 0x35:	// Reverb Delay Feedback
											sprintf(buf, "Reverb Delay Feedback: %d", s[7]);
											break;
										case 0x36:	// Reverb Send to Chorus
											sprintf(buf, "Reverb Send to Chorus: %d", s[7]);
											break;
										case 0x37:	// SC-88 Reverb Pre-Delay Time
											sprintf(buf, "SC-88 Reverb Pre-Delay Time: %d", s[7]);
											break;
										case 0x38:	// Chorus Macro
											strcpy(buf, "Chorus Macro: ");
											switch (s[7])
											{
												case 0x00: strcat(buf, "Chorus 1"); break;
												case 0x01: strcat(buf, "Chorus 2"); break;
												case 0x02: strcat(buf, "Chorus 3"); break;
												case 0x03: strcat(buf, "Chorus 4"); break;
												case 0x04: strcat(buf, "Feedback Chorus"); break;
												case 0x05: strcat(buf, "Flanger"); break;
												case 0x06: strcat(buf, "Short Delay"); break;
												case 0x07: strcat(buf, "Short Delay - Feedback"); break;
											}
											break;
										case 0x39:	// Chorus PRE-LPF
											sprintf(buf, "Chorus PRE-LPF: %d", s[7]);
											break;
										case 0x3A:	// Chorus Level
											sprintf(buf, "Chorus Level: %d", s[7]);
											break;
										case 0x3B:	// Chorus Feedback
											sprintf(buf, "Chorus Feedback: %d", s[7]);
											break;
										case 0x3C:	// Chorus Delay
											sprintf(buf, "Chorus Delay: %d", s[7]);
											break;
										case 0x3D:	// Chorus Rate
											sprintf(buf, "Chorus Rate: %d", s[7]);
											break;
										case 0x3E:	// Chorus Depth
											sprintf(buf, "Chorus Depth: %d", s[7]);
											break;
										case 0x3F:	// Chorus Send to Reverb
											sprintf(buf, "Chorus Send to Reverb: %d", s[7]);
											break;
										case 0x40:	// Chorus Send to Delay
											sprintf(buf, "Chorus Send to Delay: %d", s[7]);
											break;
										case 0x50:	// SC-88 Delay Macro
											strcpy(buf, "SC-88 Delay Macro: ");
											switch (s[7])
											{
												case 0x00: strcat(buf, "Delay 1"); break;
												case 0x01: strcat(buf, "Delay 2"); break;
												case 0x02: strcat(buf, "Delay 3"); break;
												case 0x03: strcat(buf, "Delay 4"); break;
												case 0x04: strcat(buf, "Pan Delay 1"); break;
												case 0x05: strcat(buf, "Pan Delay 2"); break;
												case 0x06: strcat(buf, "Pan Delay 3"); break;
												case 0x07: strcat(buf, "Pan Delay 4"); break;
												case 0x08: strcat(buf, "Delay to Reverb"); break;
												case 0x09: strcat(buf, "Pan Repeat"); break;
											}
											break;
										case 0x51:	// SC-88 Delay PRE-LPF
											sprintf(buf, "SC-88 Delay PRE-LPF: %d", s[7]);
											break;
										case 0x52:	// SC-88 Delay Time Center
											sprintf(buf, "SC-88 Delay Time Center: %d", s[7]);
											break;
										case 0x53:	// SC-88 Delay Time Ratio Left
											sprintf(buf, "SC-88 Delay Time Ratio Left: %d", s[7]);
											break;
										case 0x54:	// SC-88 Delay Time Ratio Right
											sprintf(buf, "SC-88 Delay Time Ratio Right: %d", s[7]);
											break;
										case 0x55:	// SC-88 Delay Level Center
											sprintf(buf, "SC-88 Delay Level Center: %d", s[7]);
											break;
										case 0x56:	// SC-88 Delay Level Left
											sprintf(buf, "SC-88 Delay Level Left: %d", s[7]);
											break;
										case 0x57:	// SC-88 Delay Level Right
											sprintf(buf, "SC-88 Delay Level Right: %d", s[7]);
											break;
										case 0x58:	// SC-88 Delay Level
											sprintf(buf, "SC-88 Delay Level: %d", s[7]);
											break;
										case 0x59:	// SC-88 Delay Feedback
											sprintf(buf, "SC-88 Delay Feedback: %s%d", s[7] > 0x40 ? "+" : "", s[7] - 0x40);
											break;
										case 0x5A:	// SC-88 Send Level to Reverb
											sprintf(buf, "SC-88 Send Level to Reverb: %d", s[7]);
											break;
									}
									break;
								case 0x02:	// SC-88 Equalization Parameter
									switch (s[6])
									{
										case 0x00:
											sprintf(buf, "SC-88 Low Freq. EQ: %d Hz", s[7] ? 200 : 100);
											break;
										case 0x01:
											sprintf(buf, "SC-88 Low Freq. Gain: %s%d db", s[7] >= 0x40 ? "+" : "", s[7] - 0x40);
											break;
										case 0x02:
											sprintf(buf, "SC-88 High Freq. EQ: %d kHz", s[7] ? 8 : 4);
											break;
										case 0x03:
											sprintf(buf, "SC-88 High Freq. Gain: %s%d db", s[7] >= 0x40 ? "+" : "", s[7] - 0x40);
											break;
									}
									break;
								case 0x03:	// SC-88 Pro EFX Parameter
									switch (s[6])
									{
										case 0x00:
											sprintf(buf, "SC-88 Pro EFX Type %d/%d", s[7], s[8]);
											break;
										default:
											sprintf(buf, "SC-88 Pro EFX Parameter %d: %d", s[6] - 2, s[7]);
											break;
									}
									break;
							}
							break;
						case 0x41:	// Drum Setup Parameter
							strcpy(buf, "Drum Setup");
							break;
						case 0x48:  // SC Bulk Dump
							strcpy(buf, "SC Bulk Dump");
							break;
						case 0x49:	// SC Bulk Dump Drum Parameters
							strcpy(buf, "SC Bulk Dump Drum Parameters");
							break;
						case 0x50:	// SC-88 Block "B" Parameter
							sprintf(buf, "SC-88 Block \"B\" SC-88 Part %d ", interpret_sysex_part(LONYBBLE(s[5])));
							goto SC88PartParam;		// Cheap but very effective
						case 0x58:	// SC-88 Block "B" Bulk Dump
							strcpy(buf, "SC-88 Block \"B\" Bulk Dump");
							break;
						case 0x59:	// SC-88 Block "B" Bulk Dump Drum Parameters
							strcpy(buf, "SC-88 Block \"B\" Bulk Dump Drum Parameters");
							break;
					}
					break;
				case 0x45:	// GS Display Data
					switch (s[4])
					{
						case 0x10:			// Sound Canvas Display
							switch (s[5])
							{
								case 0x00:	// Display Letters
									if (s[6] == 0x00)	// Letter bytes
									{
										strcpy(buf, "SC Display: ");
										j = strlen(buf);
										for (i = 7; i < len - 2; i++)
											buf[j++] = s[i];
										buf[j] = '\0';							
									}
									break;
								case 0x01:	// Display Dot Letters
									strcpy(buf, "SC Bitmap: Page 1");
									break;
								case 0x02:
									strcpy(buf, "SC Bitmap: Page 4");
									break;
								case 0x03:
									strcpy(buf, "SC Bitmap: Page 5");
									break;
								case 0x04:
									strcpy(buf, "SC Bitmap: Page 7");
									break;
								case 0x20:	// Set Display Page
									switch (s[6])
									{
										case 0x00:	// Display Page
											sprintf(buf, "Display Page: %d", s[7]);
											break;
										case 0x01:	// Display Page Time
											sprintf(buf, "Display Page Time: %.2f sec",
												(float) s[7] / 15.0f * 7.2f);
											break;
									}
									break;
							}
							break;
					}
					break;
			}
			break;
		case 0x43:	// Yamaha
			switch (s[2])						// Model ID
			{
				case 0x49:	// SW60XG Message
				case 0x7A:
					strcpy(buf, "SW60XG Message");
					break;
				case 0x4C:	// XG Message
					switch (s[3])				// XG System Parameter
					{
						case 0x00:
							switch (s[4])		// System Byte for XG System Parameter 0x00
							{
								case 0x00:
									switch (s[5])
									{
										case 0x00:	// Master Tune
											strcpy(buf, "Master Tune");
											break;
										case 0x04:	// Master Volume
											sprintf(buf, "Master Volume: %d", s[6]);
										case 0x7E:	// Set XG System On
											strcpy(buf, "XG Reset");
											break;
										case 0x7F:	// All Parameter Reset
											strcpy(buf, "All Parameter Reset");
											break;
									}
									break;
							}
							break;
						case 0x02:
							switch (s[4])		// System Byte for XG System Parameter 0x02
							{
								case 0x01:
									switch (s[5])
									{
										case 0x00:	// Reverb
											strcpy(buf, "Reverb Type: ");
											strcat(buf, get_yamaha_effect_name(s[6]));
											break;
										case 0x02:	// Reverb Time
											sprintf(buf, "Reverb Time: %d", s[6]);
											break;
										case 0x03:	// Reverb Diffusion
											sprintf(buf, "Reverb Diffusion: %d", s[6]);
											break;
										case 0x04:	// Reverb Initial Delay
											sprintf(buf, "Reverb Initial Delay: %d", s[6]);
											break;
										case 0x05:	// Reverb HPF Cutoff
											sprintf(buf, "Reverb HPF Cutoff: %d", s[6]);
											break;
										case 0x06:	// Reverb LPF Cutoff
											sprintf(buf, "Reverb LPF Cutoff: %d", s[6]);
											break;
										case 0x07:	// Reverb Width
											sprintf(buf, "Reverb Width: %d", s[6]);
											break;
										case 0x08:	// Reverb Height
											sprintf(buf, "Reverb Height: %d", s[6]);
											break;
										case 0x09:	// Reverb Depth
											sprintf(buf, "Reverb Depth: %d", s[6]);
											break;
										case 0x0A:	// Reverb Wall Vary
											sprintf(buf, "Reverb Wall Vary: %d", s[6]);
											break;
										case 0x0B:	// Reverb Dry/Wet
											sprintf(buf, "Reverb Dry/Wet: %s%d", (s[6] == 64) ? "" : (s[6] < 64 ? "D" : "W"), abs(s[6] - 64));
											break;
										case 0x0C:	// Reverb Level
											sprintf(buf, "Reverb Level: %d", s[6]);
											break;
										case 0x0D:	// Reverb Pan
											sprintf(buf, "Reverb Pan: %s%d", (s[6] == 64) ? "" : (s[6] < 64 ? "L" : "R"), abs(s[6] - 64));
											break;
										case 0x20:	// Chorus
											strcpy(buf, "Chorus Type: ");
											strcat(buf, get_yamaha_effect_name(s[6]));
											break;
										case 0x22:	// Chorus LFO Frequency
											sprintf(buf, "Chorus LFO Frequency: %d", s[6]);
											break;
										case 0x23:	// Chorus LFO Depth
											sprintf(buf, "Chorus LFO Depth: %d", s[6]);
											break;
										case 0x24:	// Chorus Feedback
											sprintf(buf, "Chorus Feedback: %s%d", s[6] > 0x40 ? "+" : "", s[6] - 0x40);
											break;
										case 0x25:	// Chorus Delay Offset
											sprintf(buf, "Chorus Delay Offset: %s%d", s[6] > 0x40 ? "+" : "", s[6] - 0x40);
											break;
										case 0x26:	// Chorus Parameter 5
											sprintf(buf, "Chorus Parameter 5: %d", s[6]);
											break;
										case 0x27:	// Chorus EQ Low Frequency
											sprintf(buf, "Chorus EQ Low Frequency: %d", s[6]);
											break;
										case 0x28:	// Chorus EQ Low Gain
											sprintf(buf, "Chorus EQ Low Gain: %d", s[6] - 64);
											break;
										case 0x29:	// Chorus EQ High Frequency
											sprintf(buf, "Chorus EQ High Frequency: %d", s[6]);
											break;
										case 0x2A:	// Chorus EQ High Gain
											sprintf(buf, "Chorus EQ High Gain: %d", s[6] - 64);
											break;
										case 0x2B:	// Chorus Dry/Wet
											sprintf(buf, "Chorus Dry/Wet: %d", s[6] - 64);
											break;
										case 0x2C:	// Chorus Level
											sprintf(buf, "Chorus Level: %d", s[6]);
											break;
										case 0x2D:	// Chorus Pan
											sprintf(buf, "Chorus Pan: %s%d", (s[6] == 64) ? "" : (s[6] < 64 ? "L" : "R"), abs(s[6] - 64));
											break;
										case 0x2E:	// Chorus Send to Reverb
											sprintf(buf, "Chorus Send to Reverb: %d", s[6]);
											break;
										case 0x40:	// Variation
											strcpy(buf, "Variation Type: ");
											strcat(buf, get_yamaha_effect_name(s[6]));
											break;
										case 0x42:	// Variation Parameter 1
											sprintf(buf, "Variation Parameter 1: %d", s[7]);
											break;
										case 0x44:	// Variation Parameter 2
											sprintf(buf, "Variation Parameter 2: %d", s[7]);
											break;
										case 0x46:	// Variation Parameter 3
											sprintf(buf, "Variation Parameter 3: %d", s[7]);
											break;
										case 0x48:	// Variation Parameter 4
											sprintf(buf, "Variation Parameter 4: %d", s[7]);
											break;
										case 0x4A:	// Variation Parameter 5
											sprintf(buf, "Variation Parameter 5: %d", s[7]);
											break;
										case 0x4C:	// Variation Parameter 6
											sprintf(buf, "Variation Parameter 6: %d", s[7]);
											break;
										case 0x4E:	// Variation Parameter 7
											sprintf(buf, "Variation Parameter 7: %d", s[7]);
											break;
										case 0x50:	// Variation Parameter 8
											sprintf(buf, "Variation Parameter 8: %d", s[7]);
											break;
										case 0x52:	// Variation Parameter 9
											sprintf(buf, "Variation Parameter 9: %d", s[7]);
											break;
										case 0x54:	// Variation Parameter 10
											sprintf(buf, "Variation Parameter 10: %d", s[7]);
											break;
										case 0x56:	// Variation Level
											sprintf(buf, "Variation Level: %d", s[6]);
											break;
										case 0x57:	// Variation Pan
											sprintf(buf, "Variation Pan: %s%d", (s[6] == 64) ? "" : (s[6] < 64 ? "L" : "R"), abs(s[6] - 64));
											break;
										case 0x58:	// Variation Send to Reverb
											sprintf(buf, "Variation Send to Reverb: %d", s[6]);
											break;
										case 0x59:	// Variation Send to Chorus
											sprintf(buf, "Variation Send to Chorus: %d", s[6]);
											break;
										case 0x5A:	// Variation Connection
											sprintf(buf, "Variation Connection: %s", s[6] ? "System" : "Insertion");
											break;
										case 0x5B:	// Variation Part
											sprintf(buf, "Variation Part: 0x%02X", s[6]);
											break;
									}
									break;
								case 0x40:
									switch (s[5])
									{
										case 0x00:	// EQ Type
											strcpy(buf, "EQ Type: ");
											switch (s[6])
											{
												case 0x00: strcat(buf, "Flat"); break;
												case 0x01: strcat(buf, "Jazz"); break;
												case 0x02: strcat(buf, "Pops"); break;
												case 0x03: strcat(buf, "Rock"); break;
												case 0x04: strcat(buf, "Classic"); break;
												default: sprintf(&buf[strlen(buf)], "Unknown (%02X)", s[6]); break;
											}
											break;
										case 0x01:	// EQ Gain 1
											sprintf(buf, "EQ Gain 1: %d", s[6]);
											break;
										case 0x02:	// EQ Frequency 1
											sprintf(buf, "EQ Frequency 1: %d", s[6]);
											break;
										case 0x03:	// EQ Q1
											sprintf(buf, "EQ Q1: %d", s[6]);
											break;
										case 0x04:	// EQ Shape 1
											sprintf(buf, "EQ Shape 1: %s", s[6] ? "Peaking" : "Shelving");
											break;
										case 0x05:	// EQ Gain 2
											sprintf(buf, "EQ Gain 2: %d", s[6]);
											break;
										case 0x06:	// EQ Frequency 2
											sprintf(buf, "EQ Frequency 2: %d", s[6]);
											break;
										case 0x07:	// EQ Q2
											sprintf(buf, "EQ Q2: %d", s[6]);
											break;
										case 0x09:	// EQ Gain 3
											sprintf(buf, "EQ Gain 3: %d", s[6]);
											break;
										case 0x0A:	// EQ Frequency 3
											sprintf(buf, "EQ Frequency 3: %d", s[6]);
											break;
										case 0x0B:	// EQ Q3
											sprintf(buf, "EQ Q3: %d", s[6]);
											break;
										case 0x0D:	// EQ Gain 4
											sprintf(buf, "EQ Gain 4: %d", s[6]);
											break;
										case 0x0E:	// EQ Frequency 4
											sprintf(buf, "EQ Frequency 4: %d", s[6]);
											break;
										case 0x0F:	// EQ Q4
											sprintf(buf, "EQ Q4: %d", s[6]);
											break;
										case 0x11:	// EQ Gain 5
											sprintf(buf, "EQ Gain 5: %d", s[6]);
											break;
										case 0x12:	// EQ Frequency 5
											sprintf(buf, "EQ Frequency 5: %d", s[6]);
											break;
										case 0x13:	// EQ Q5
											sprintf(buf, "EQ Q5: %d", s[6]);
											break;
										case 0x14:	// EQ Shape 5
											sprintf(buf, "EQ Shape 5: %s", s[6] ? "Peaking" : "Shelving");
											break;
									}
									break;
							}
							break;
						case 0x03:
							switch (s[4])		// System Byte for XG System Parameter 0x03
							{
								case 0x00:
								case 0x01:	// ??? Not sure if 0x01 should be included, see dajir1.mid
									switch (s[5])
									{
										case 0x00:	// Insertion Effect
											strcpy(buf, "Insertion Effect: ");
											strcat(buf, get_yamaha_effect_name(s[6]));
											break;
										case 0x0B:	// Insertion Parameter 10
											sprintf(buf, "Insertion Parameter 10: %d", s[6]);
											break;
										case 0x0C:	// Insertion Effect Part
											sprintf(buf, "Insertion Effect Part: %d", s[6]);
											break;
									}
									break;
							}
							break;
						case 0x06:	// XG Display
/*							switch (s[5])
							{
								case 0x11:	// Displayed Letters
								case 0x12:	// Displayed Letters
								case 0x13:	// Displayed Letters*/
									strcpy(buf, "Displayed Letters: ");
									j = strlen(buf);
									for (i = 6; i < len - 1; i++)
										buf[j++] = s[i];
									buf[j] = '\0';
									break;
//							}
							break;
						case 0x08:	// XG Part Parameter
							sprintf(buf, "Part %d ", s[4] + 1);
							switch (s[5])
							{
								case 0x00:	// Element Reserve
									sprintf(&buf[strlen(buf)], "Element Reserve: %d", s[6]);
									break;
								case 0x01:	// Bank Select MSB
									sprintf(&buf[strlen(buf)], "Bank Select MSB: %d", s[6]);
									break;
								case 0x02:	// Bank Select LSB
									sprintf(&buf[strlen(buf)], "Bank Select LSB: %d", s[6]);
									break;
								case 0x03:	// Program Number
									sprintf(&buf[strlen(buf)], "Program Number: %d (%s)", s[6], get_program_name(s[6], 0));
									break;
								case 0x04:	// Rx Channel
									sprintf(&buf[strlen(buf)], "Rx Channel: %d", s[6]);
									break;
								case 0x05:	// Mono/Poly Mode
									sprintf(&buf[strlen(buf)], "Mono/Poly Mode: %s", s[6] ? "Poly" : "Mono");
									break;
								case 0x06:	// Same Note Number Assign
									sprintf(&buf[strlen(buf)], "Same Note Number Assign: ");
									switch (s[6])
									{
										case 0x00: strcat(buf, "Single"); break;
										case 0x01: strcat(buf, "Multi"); break;
										case 0x02: strcat(buf, "Inst (for Drum)"); break;
										default: sprintf(&buf[strlen(buf)], "Unknown (%d)", s[6]);
									}
									break;
								case 0x07:	// Part Mode
									strcat(buf, "Part Mode: ");
									switch (s[6])
									{
										case 0x00: strcat(buf, "Normal"); break;
										case 0x01: strcat(buf, "Drum"); break;
										case 0x02: strcat(buf, "Drums 1"); break;
										case 0x03: strcat(buf, "Drums 2"); break;
										case 0x04: strcat(buf, "Drums 3"); break;
										case 0x05: strcat(buf, "Drums 4"); break;
										default: sprintf(&buf[strlen(buf)], "Unknown (%02X)", s[6]); break;
									}
									break;
								case 0x08:	// Note Shift
									sprintf(&buf[strlen(buf)], "Note Shift: %s%d semitones", s[6] > 0x40 ? "+" : "", s[6] - 0x40);
									break;
								case 0x09:	// Detune
									sprintf(&buf[strlen(buf)], "Detune: %d/%d", s[6], s[7]);
									break;
								case 0x0B:	// Volume
									sprintf(&buf[strlen(buf)], "Volume: %d", s[6]);
									break;
								case 0x0C:	// Velocity Sense Depth
									sprintf(&buf[strlen(buf)], "Velocity Sense Depth: %d", s[6]);
									break;
								case 0x0D:	// Velocity Sense Offset
									sprintf(&buf[strlen(buf)], "Velocity Sense Offset: %d", s[6]);
									break;
								case 0x0E:	// Pan
									strcat(buf, "Pan: ");
									if (!s[6])
										strcat(buf, "Random");
									else
									if (s[6] < 64)
										sprintf(&buf[strlen(buf)], "L%d", 64 - s[6]);
									else
									if (s[6] == 64)
										sprintf(&buf[strlen(buf)], "0");
									else
										sprintf(&buf[strlen(buf)], "R%d", s[6] - 64);
									break;
								case 0x11:	// Dry Level
									sprintf(&buf[strlen(buf)], "Dry Level: %d", s[6]);
									break;
								case 0x12:	// Chorus Level
									sprintf(&buf[strlen(buf)], "Chorus Level: %d", s[6]);
									break;
								case 0x13:	// Reverb Level
									sprintf(&buf[strlen(buf)], "Reverb Level: %d", s[6]);
									break;
								case 0x14:	// Variation Level
									sprintf(&buf[strlen(buf)], "Variation Level: %d", s[6]);
									break;
								case 0x15:	// Vibrato Rate
									sprintf(&buf[strlen(buf)], "Vibrato Rate: %d", s[6]);
									break;
								case 0x16:	// Vibrato Depth
									sprintf(&buf[strlen(buf)], "Vibrato Depth: %d", s[6]);
									break;
								case 0x17:	// Vibrato Delay
									sprintf(&buf[strlen(buf)], "Vibrato Delay: %d", s[6]);
									break;
								case 0x18:	// Filter Cutoff
									sprintf(&buf[strlen(buf)], "Filter Cutoff: %d", s[6]);
									break;
								case 0x19:	// Filter Resonance
									sprintf(&buf[strlen(buf)], "Filter Resonance: %d", s[6]);
									break;
								case 0x1A:	// Attack Time
									sprintf(&buf[strlen(buf)], "Attack Time: %d", s[6]);
									break;
								case 0x1B:	// Decay Time
									sprintf(&buf[strlen(buf)], "Decay Time: %d", s[6]);
									break;
								case 0x1C:	// Release Time
									sprintf(&buf[strlen(buf)], "Release Time: %d", s[6]);
									break;
								case 0x1D:	// Mod Pitch Control
									sprintf(&buf[strlen(buf)], "Mod Pitch Control: %d", s[6]);
									break;
								case 0x1E:	// Mod Filter Control
									sprintf(&buf[strlen(buf)], "Mod Filter Control: %d", s[6]);
									break;
								case 0x1F:	// Mod Amplitude Control
									sprintf(&buf[strlen(buf)], "Mod Amplitude Control: %d", s[6]);
									break;
								case 0x20:	// LFO PMOD Depth
									sprintf(&buf[strlen(buf)], "LFO PMOD Depth: %d", s[6]);
									break;
								case 0x21:	// LFO FMOD Depth
									sprintf(&buf[strlen(buf)], "LFO FMOD Depth: %d", s[6]);
									break;
								case 0x22:	// LFO AMOD Depth
									sprintf(&buf[strlen(buf)], "LFO AMOD Depth: %d", s[6]);
									break;
								case 0x23:	// Bend Pitch Control
									sprintf(&buf[strlen(buf)], "Bend Pitch Control: %d", s[6]);
									break;
								case 0x24:	// Bend Filter Control
									sprintf(&buf[strlen(buf)], "Bend Filter Control: %d", s[6]);
									break;
								case 0x25:	// Bend Amplitude Control
									sprintf(&buf[strlen(buf)], "Bend Amplitude Control: %d", s[6]);
									break;
								case 0x26:	// Bend LFO PMOD Depth
									sprintf(&buf[strlen(buf)], "Bend LFO PMOD Depth: %d", s[6]);
									break;
								case 0x27:	// Bend LFO FMOD Depth
									sprintf(&buf[strlen(buf)], "Bend LFO FMOD Depth: %d", s[6]);
									break;
								case 0x28:	// Bend LFO AMOD Depth
									sprintf(&buf[strlen(buf)], "Bend LFO AMOD Depth: %d", s[6]);
									break;
								case 0x69:	// Pitch Initial Level
									sprintf(&buf[strlen(buf)], "Pitch Initial Level: %d", s[6] - 64);
									break;
								case 0x6A:	// Pitch Attack Time
									sprintf(&buf[strlen(buf)], "Pitch Attack Time: %d", s[6] - 64);
									break;
								case 0x6B:	// Pitch Release Level
									sprintf(&buf[strlen(buf)], "Pitch Release Level: %d", s[6] - 64);
									break;
								case 0x6C:	// Pitch Release Time
									sprintf(&buf[strlen(buf)], "Pitch Release Time: %d", s[6] - 64);
									break;
							}
							break;
						case 0x30:	// XG Drum Parameter
						case 0x31:
							sprintf(buf, "Drum Note %d Parameter %d = %d", s[4], s[5], s[6]);
							break;
					}
					break;
			}

			break;
		case 0x7E:	// General MIDI
		case 0x7F:
			// F0 7E 7F 09 01 F7			(definitely standard GM Reset)
			// F0 7F 7F 04 01 00 7F F7		(??  from FF7 MIDI's)
			// F0H,7FH,7FH,04H,01H,llH,mmH,F7H
			// F0 7F 7F 04 01 00 7E F7
			if (s[1] == 0x7F && 
				s[2] == 0x09 && 
				s[3] == 0x01)
				strcpy(buf, "GM Reset");
			else
			if (s[1] == 0x7F &&
				s[2] == 0x04 &&
				s[3] == 0x01)
				sprintf(buf, "Master Volume: %d", s[5]);
			break;
	}

	return buf;
}

// Interprets a sysex model ID
char *interpret_model(unsigned char *s)
{
	static char buf[256];

	buf[0] = '\0';

	switch (s[0])								// Manufacturer ID
	{
		case 0x41:	// Roland
			switch (s[2])
			{
				case 0x16:	// MT-32
					strcpy(buf, "MT-32");
					break;
				case 0x42:	// GS Message
					strcpy(buf, "GS Msg");
					break;
				case 0x45:	// GS Display Data
					strcpy(buf, "GS Disp");
					break;
			}
			break;
		case 0x43:	// Yamaha
			switch (s[2])
			{
				case 0x49:	// SW60XG
				case 0x7A:
					strcpy(buf, "SW60XG");
					break;
				case 0x4C:	// XG Message
					strcpy(buf, "XG Msg");
					break;
			}
			break;
	}

	if (!buf[0])
		sprintf(buf, "0x%02X", (unsigned int) s[2]);

	return buf;
}

void check_midi_standard(unsigned char *data)
{
	switch (data[0])								// Manufacturer ID
	{
		case 0x41:	// Roland
			switch (data[2])
			{
				case 0x16:	// MT-32
					ms.midi_standard = MIDI_STANDARD_MT32;
					break;
				case 0x42:	// GS Message
					ms.midi_standard = MIDI_STANDARD_GS;
					break;
			}
			break;
		case 0x43:	// Yamaha
			switch (data[2])
			{
				case 0x4C:	// XG Message
					ms.midi_standard = MIDI_STANDARD_XG;
					break;
			}
			break;
		case 0x7E:	// General MIDI
		case 0x7F:
			ms.midi_standard = MIDI_STANDARD_GM;
			break;
	}
}

// Interprets a sysex command ID
char *interpret_command(unsigned char *s)
{
	static char buf[256];

	buf[0] = '\0';

	switch (s[0])								// Manufacturer ID
	{
		case 0x41:	// Roland
			switch (s[3])						// Command ID
			{
				case 0x11:	// RQ1
					strcpy(buf, "RQ1");
					break;
				case 0x12:	// DT1
					strcpy(buf, "DT1");
					break;
			}
			break;
	}

	if (!buf[0])
		sprintf(buf, "0x%02X", (unsigned int) s[3]);

	return buf;
}

// Interprets a part number from a sysex string
char interpret_sysex_part(unsigned char c)
{
	char lo = LONYBBLE(c);

	if (lo == 0x00)
		return 10;
	if (lo < 0x0A)
		return lo;
	return lo + 1;
}

char *get_yamaha_effect_name(unsigned char effect)
{
	static char buf[64];

	switch (effect)
	{
		case 0x00: strcpy(buf, "No Effect"); break;
		case 0x01: strcpy(buf, "Hall1"); break;
		case 0x02: strcpy(buf, "Room1"); break;
		case 0x03: strcpy(buf, "Stage1"); break;
		case 0x04: strcpy(buf, "Plate"); break;
		case 0x05: strcpy(buf, "Delay L,C,R"); break;
		case 0x06: strcpy(buf, "Delay L,R"); break;
		case 0x07: strcpy(buf, "Echo"); break;
		case 0x08: strcpy(buf, "Cross Delay"); break;
		case 0x09: strcpy(buf, "ER1"); break;
		case 0x0A: strcpy(buf, "Gate Reverb"); break;
		case 0x0B: strcpy(buf, "Reverse Gate"); break;
		case 0x10: strcpy(buf, "White Room"); break;
		case 0x11: strcpy(buf, "Tunnel"); break;
		case 0x12: strcpy(buf, "Canyon"); break;
		case 0x13: strcpy(buf, "Basement"); break;
		case 0x14: strcpy(buf, "Karaoke1"); break;
		case 0x41: strcpy(buf, "Chorus1"); break;
		case 0x42: strcpy(buf, "Celeste1"); break;
		case 0x43: strcpy(buf, "Flanger1"); break;
		case 0x44: strcpy(buf, "Symphonic"); break;
		case 0x45: strcpy(buf, "Rotary Speaker"); break;
		case 0x46: strcpy(buf, "Tremolo"); break;
		case 0x47: strcpy(buf, "Auto Pan"); break;
		case 0x48: strcpy(buf, "Phaser1"); break;
		case 0x49: strcpy(buf, "Distortion"); break;
		case 0x4A: strcpy(buf, "Over Drive"); break;
		case 0x4B: strcpy(buf, "Amp Simulator"); break;
		case 0x4C: strcpy(buf, "3-Band EQ"); break;
		case 0x4D: strcpy(buf, "2-Band EQ"); break;
		case 0x4E: strcpy(buf, "Auto Wah (LFO)"); break;
		case 0x50: strcpy(buf, "Pitch Change"); break;
		case 0x51: strcpy(buf, "Aural Exciter"); break;
		case 0x52: strcpy(buf, "Touch Wah"); break;
		case 0x53: strcpy(buf, "Compressor"); break;
		case 0x54: strcpy(buf, "Noise Gate"); break;
		case 0x55: strcpy(buf, "Voice Cancel"); break;
		default: sprintf(buf, "Unknown (%02X)", effect);
	}

	return buf;
}

////////////// BEGIN TOOLTIP CODE

// DoCreateDialogTooltip - creates a tooltip control for a dialog box, 
//     enumerates the child control windows, and installs a hook 
//     procedure to monitor the message stream for mouse messages posted 
//     to the control windows. 
// Returns TRUE if successful, or FALSE otherwise. 
// 
// Global variables 
// ghInstance - handle to the application instance. 
// g_hwndTT - handle to the tooltip control. 
// hwndApp - handle to the dialog box. 
// g_hhk - handle to the hook procedure. 
 
BOOL DoCreateDialogTooltip(void) 
{ 

    // Ensure that the common control DLL is loaded, and create 
    // a tooltip control. 
    InitCommonControls(); 
    g_hwndTT = CreateWindowEx(0, TOOLTIPS_CLASS, (LPSTR) NULL, 
        TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
        CW_USEDEFAULT, hwndApp, (HMENU) NULL, ghInstance, NULL); 
 
    if (g_hwndTT == NULL) 
        return FALSE; 
 
    // Enumerate the child windows to register them with the tooltip
    // control. 
    if (!EnumChildWindows(hwndApp, (WNDENUMPROC) EnumChildProc, 0)) 
        return FALSE; 
 
    // Install a hook procedure to monitor the message stream for mouse 
    // messages intended for the controls in the dialog box. 
    g_hhk = SetWindowsHookEx(WH_GETMESSAGE, GetMsgProc, 
        (HINSTANCE) NULL, GetCurrentThreadId()); 
 
    if (g_hhk == (HHOOK) NULL) 
        return FALSE; 
 
    return TRUE; 
} 
 
// EmumChildProc - registers control windows with a tooltip control by
//     using the TTM_ADDTOOL message to pass the address of a 
//     TOOLINFO structure. 
// Returns TRUE if successful, or FALSE otherwise. 
// hwndCtrl - handle of a control window. 
// lParam - application-defined value (not used). 
WNDENUMPROC EnumChildProc(HWND hwndCtrl, LPARAM lParam) 
{ 
    TOOLINFO ti; 
    char szClass[64]; 
 
    // Skip static controls. 
    GetClassName(hwndCtrl, szClass, sizeof(szClass)); 
    if (lstrcmpi(szClass, "STATIC")) { 
        ti.cbSize = sizeof(TOOLINFO); 
        ti.uFlags = TTF_IDISHWND; 
        ti.hwnd = hwndApp; 
        ti.uId = (UINT) hwndCtrl; 
        ti.hinst = 0; 
        ti.lpszText = LPSTR_TEXTCALLBACK; 
        SendMessage(g_hwndTT, TTM_ADDTOOL, 0, 
            (LPARAM) (LPTOOLINFO) &ti); 
    } 
    //return (int (__stdcall *)(struct HWND__ *,long)) TRUE; 
	return (WNDENUMPROC) TRUE;
}
 
// GetMsgProc - monitors the message stream for mouse messages intended 
//     for a control window in the dialog box. 
// Returns a message-dependent value. 
// nCode - hook code. 
// wParam - message flag (not used). 
// lParam - address of an MSG structure. 
LRESULT CALLBACK GetMsgProc(int nCode, WPARAM wParam, LPARAM lParam) 
{ 
    MSG *lpmsg; 
 
    lpmsg = (MSG *) lParam; 
    if (nCode < 0 || !(IsChild(hwndApp, lpmsg->hwnd))) 
        return (CallNextHookEx(g_hhk, nCode, wParam, lParam)); 
 
    switch (lpmsg->message) { 
        case WM_MOUSEMOVE: 
        case WM_LBUTTONDOWN: 
        case WM_LBUTTONUP: 
        case WM_RBUTTONDOWN: 
        case WM_RBUTTONUP: 
            if (g_hwndTT != NULL) { 
                MSG msg; 
 
                msg.lParam = lpmsg->lParam; 
                msg.wParam = lpmsg->wParam; 
                msg.message = lpmsg->message; 
                msg.hwnd = lpmsg->hwnd; 
                SendMessage(g_hwndTT, TTM_RELAYEVENT, 0, 
                    (LPARAM) (LPMSG) &msg); 
            } 
            break; 
        default: 
            break; 
    } 
    return (CallNextHookEx(g_hhk, nCode, wParam, lParam)); 
} 
 
// OnWMNotify - provides the tooltip control with the appropriate text 
//     to display for a control window. This function is called by 
//     the dialog box procedure in response to a WM_NOTIFY message. 
// lParam - second message parameter of the WM_NOTIFY message. 
VOID OnWMNotify(LPARAM lParam) 
{ 
    LPTOOLTIPTEXT lpttt; 
    int idCtrl; 
 
    if ((((LPNMHDR) lParam)->code) == TTN_NEEDTEXT) { 
        idCtrl = GetDlgCtrlID((HWND) ((LPNMHDR) lParam)->idFrom); 
        lpttt = (LPTOOLTIPTEXT) lParam; 
 
        switch (idCtrl)
		{
            case IDC_MIDI_OUT:
                lpttt->lpszText = "Bite me!";
                break;
        }
    }
	return;
}

void CleanupTooltip(void)
{
	if (g_hhk)
	{
		UnhookWindowsHookEx(g_hhk);
		g_hhk = NULL;
	}
}
////////////// END TOOLTIP CODE

// Case insensitive string search - finds target in src, returns 
// pointer to beginning of match or NULL if no match found
char *stristr(char *src, char *target)
{
	char *found = NULL;	// pointer to beginning of matched string
	char *sp;			// search pointer
	char *tp;			// target pointer

	// We don't like null pointers
	if (!src || !target)
		return NULL;

	// Loop through the source text
	while (*src)
	{
		sp = src;
		tp = target;
		found = NULL;
		// Search for an exact match from the current point in src
		while (*sp && *tp)
		{
			// Compare what we are looking at and what we want
			if (tolower(*sp) == tolower(*tp))
			{
				// If we match two chars for the first time, mark this spot
				if (!found)
					found = sp;
			}
			else
			{
				// If we haven't reached the end of the target..
				if (*tp)
					found = NULL;	// ...we haven't found the target
				break;
			}
			// Move on to compare the next chars
			sp++;
			tp++;
		}
		// If we found a matching string, return a pointer to its start
		if (found && !(*tp))
			return found;
		// Move on to the next char in src
		src++;
	}

	// We've reached the end of src without finding anything
	return NULL;
}

// Dumps the given file directly to the MIDI-out device
int handle_sysex_dump(FILE *fp)
{
	MIDIHDR mh;
	char *buf;
	char text[MAX_PATH];
	int filelen, i;
	double startTime, endTime, timelen, speed;

	// Read the file
	sprintf(text, "Reading sysex dump file %s...", ms.filename);
	SetWindowText(hwndStatusBar, text);
	filelen = _filelength(_fileno(fp));
	if (!filelen)
	{
		MessageBox(hwndApp, "Sysex file is empty!", "TMIDI Error", MB_ICONERROR);
		return 1;
	}
	buf = (char *) malloc(filelen + 1);
	fread(buf, filelen, 1, fp);
	fclose(fp);

	// Open the MIDI-out device
	if (!hout)
		init_midi_out(GetDlgItem(hwndApp, IDC_MIDI_OUT));
	if (!hout)
	{
		MessageBox(hwndApp, "Unable to open MIDI-out device for sysex dump!", "TMIDI Error", MB_ICONERROR);
		return 1;
	}

	// Reset the MIDI-out device
	midiOutReset(hout);

	// Prepare the MIDI out header
	memset(&mh, 0, sizeof(mh));
	mh.lpData = buf;
	mh.dwBufferLength = filelen;
	mh.dwBytesRecorded = filelen;
	// Prepare the sysex buffer for output
	strcpy(text, "Preparing MIDI out header...");
	SetWindowText(hwndStatusBar, text);
	midiOutPrepareHeader(hout, &mh, sizeof(mh));

	// Send the sysex buffer!
	sprintf(text, "Sending %s (%d bytes)...", ms.filename, filelen);
	SetWindowText(hwndStatusBar, text);
	printf("Sending sysex data...\n");
	startTime = GetHRTickCount();
	midiOutLongMsg(hout, &mh, sizeof(mh));
	endTime = GetHRTickCount();
	printf("Sent sysex data.\n");

	// Unprepare the sysex buffer
	do
	{
		i = midiOutUnprepareHeader(hout, &mh, sizeof(mh));
		Sleep(50);
	} while (i == MIDIERR_STILLPLAYING);

	timelen = (endTime - startTime) / 1000.0f;
	if (timelen == 0.0f)
		speed = 0.0f;
	else
		speed = (double) filelen / timelen;
	sprintf(text, "Finished sending %s (%d bytes) in %.1f seconds (%.0f bytes per second)", 
		extract_filename(ms.filename), filelen, timelen, speed);
	SetWindowText(hwndStatusBar, text);

	// Close the MIDI-out device
	close_midi_out();

	return 0;
}

char *extract_filename(char *filename)
{
	char *ch = strrchr(filename, '\\');
	if (ch)
		return ch + 1;
	else
		return filename;
}

// Interprets patch memory sysex for the MT-32
void interpret_mt32_patch_memory(unsigned char *s, int len)
{
	int addr;
	unsigned char *p = s + 7;
	unsigned char ch, group, num;
	int patch;
	FILE *fp;

	//fp = fopen("k:\\projects\\tmidi\\patchmem.txt", "a");
	addr = (s[5] << 7) | s[6];
	//fprintf(fp, "\nStarting address: %d\n", addr);

	while (p - s < len - 9)
	{
		patch = addr / 8;
		//fprintf(fp, "\nAddr %d, patch %d:\n", addr, patch);
		group = ch = *p++;
		/*fprintf(fp, "   Timbre group: ");
		switch (ch)
		{
			case 0: fprintf(fp, "Group A\n"); break;
			case 1: fprintf(fp, "Group B\n"); break;
			case 2: fprintf(fp, "Memory\n"); break;
			case 3: fprintf(fp, "Rhythm\n"); break;
			default: fprintf(fp, "Unknown (%d)\n", ch);
		}*/
		mt32_patch_groups[patch] = ch;
		num = ch = *p++;
		//fprintf(fp, "  Timbre number: %d\n", ch);
		//fprintf(fp, " Program number: %d\n", group == 1 ? num + 64 : num);
		mt32_patch_programs[patch] = group == 1 ? num + 64 : num;
		ch = *p++;
		//fprintf(fp, "      Key shift: %d\n", (signed int) ch - 24);
		ch = *p++;
		//fprintf(fp, "      Fine tune: %d\n", (signed int) ch - 50);
		ch = *p++;
		//fprintf(fp, "   Bender range: %d\n", ch);
		ch = *p++;
		//fprintf(fp, "    Assign mode: Poly %d\n", ch + 1);
		ch = *p++;
		//fprintf(fp, "  Reverb switch: %s\n", ch ? "On" : "Off");
		ch = *p++;
		//fprintf(fp, "          Dummy: %d\n", ch);
		
		addr += 8;
	}

	//fclose(fp);
}

// Interprets timbre memory sysex for the MT-32
void interpret_mt32_timbre_memory(unsigned char *s, int len)
{
	int timbre;

	timbre = s[5] / 2;
	mt32_memory_names[timbre][0] = '\0';
	strncat(mt32_memory_names[timbre], (char *) &s[7], 10);
}

// Initializes MT-32 state values on program startup
void init_mt32_state(void)
{
	int i;

	for (i = 0; i < 128; i++)
	{
		if (i < 64)
			mt32_patch_groups[i] = 0;
		else
			mt32_patch_groups[i] = 1;
		mt32_patch_programs[i] = (char) i;
	}

	for (i = 0; i < 64; i++)
		mt32_memory_names[i][0] = '\0';
}

// Returns a program name, given a program number, taking into account 
// current mode of operation and state values.
char *get_program_name(unsigned char program, unsigned char bank)
{
	static char name[256];

	assert(program >= 0 && program <= 127);

	switch (ms.midi_standard)
	{
		case MIDI_STANDARD_MT32:
			switch (mt32_patch_groups[program])
			{
				case 2:		// Memory patch
					if (mt32_patch_programs[program] < 64)
						return mt32_memory_names[mt32_patch_programs[program]];
				case 3:
					sprintf(name, "Rhythm patch %d", mt32_patch_programs[program]);
					return name;
				default:
					return mt32_program_names[mt32_patch_programs[program]];
			}
		default:
			if (bank == 0 || bank == 255)
				return gm_program_names[program];
			sprintf(name, "%s - BK%d", gm_program_names[program], bank);
			return name;
	}
}

/*INT_PTR CALLBACK MinimalDlg(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	switch (iMsg)
	{
		case WM_INITDIALOG:
		{
			return TRUE;
		}
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDOK:
					PostMessage(hDlg, WM_CLOSE, 0, 0);
					break;
				case IDCANCEL:
					PostMessage(hDlg, WM_CLOSE, 0, 0);
					break;
			}
			break;
		case WM_CLOSE:
		{
			EndDialog(hDlg, FALSE);
			break;
		}
	}

	return FALSE;
}*/

// Dialog for output device configuration window
INT_PTR CALLBACK OutConfigDlg(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	midi_device_t *dev;
	static HWND hwndLB = NULL;
	static midi_device_t **devs = NULL;
	int i = 0;
	char buf[256];
	
	switch (iMsg)
	{
		case WM_INITDIALOG:
		{
			hwndLB = GetDlgItem(hDlg, IDC_OUT_DEV_LIST);
			
			// Count the # of devices
			i = 0;
			for (dev = midi_devices; dev; dev = dev->next)
				if (!dev->input_device)
					i++;
			// Allocate a static array of device pointers
			devs = (midi_device_t **) malloc(sizeof(midi_device_t *) * i);
			// Add the devices to the list box
			i = 0;
			for (dev = midi_devices; dev; dev = dev->next)
			{
				if (!dev->input_device)
				{
					devs[i++] = dev;
					SendMessage(hwndLB, LB_ADDSTRING, 0, (LPARAM) dev->user_device_name);
				}
			}

			PostMessage(hwndLB, LB_SETSEL, TRUE, 0);

			return TRUE;
		}
		case WM_CLOSE:
		{
			EndDialog(hDlg, 0);
			break;
		}
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDC_OUT_DEV_LIST:
					if (HIWORD(wParam) == LBN_SELCHANGE)
					{
						i = SendMessage(hwndLB, LB_GETCURSEL, 0, 0);
						dev = devs[i];
						SetDlgItemText(hDlg, IDC_DEV_NAME, dev->user_device_name);
						CheckDlgButton(hDlg, IDC_USABLE, dev->usable);
						sprintf(buf, "%d", dev->outcaps.wMid);
						SetDlgItemText(hDlg, IDC_DEV_MANUFACTURER, buf);
						sprintf(buf, "%d (driver v%d.%d)", dev->outcaps.wPid, 
							HIBYTE(dev->outcaps.vDriverVersion), LOBYTE(dev->outcaps.vDriverVersion));
						SetDlgItemText(hDlg, IDC_DEV_ID, buf);
						switch (dev->outcaps.wTechnology)
						{
							case MOD_MIDIPORT: strcpy(buf, "MIDI Port"); break;
							case MOD_SYNTH: strcpy(buf, "Synthesizer"); break;
							case MOD_SQSYNTH: strcpy(buf, "Square Wave Synthesizer"); break;
							case MOD_FMSYNTH: strcpy(buf, "FM Synthesizer"); break;
							case MOD_MAPPER: strcpy(buf, "MIDI Mapper"); break;
							case 6: strcpy(buf, "Wavetable Synthesizer"); break;
							case 7: strcpy(buf, "Software Synthesizer"); break;
							default: sprintf(buf, "Unknown (%d)", dev->outcaps.wTechnology); break;
						}
						SetDlgItemText(hDlg, IDC_DEV_TYPE, buf);
						// MIDI standard flags
						CheckDlgButton(hDlg, IDC_MS_MT32, dev->standards & MIDI_STANDARD_MT32);
						CheckDlgButton(hDlg, IDC_MS_GM, dev->standards & MIDI_STANDARD_GM);
						CheckDlgButton(hDlg, IDC_MS_GS, dev->standards & MIDI_STANDARD_GS);
						CheckDlgButton(hDlg, IDC_MS_XG, dev->standards & MIDI_STANDARD_XG);
					}
					break;
				case IDOK:
				case IDCANCEL:
					PostMessage(hDlg, WM_CLOSE, 0, 0);
					break;
			}
			break;
	}

	return FALSE;
}

void init_gdi_resources(void)
{
	unsigned char fg = 208, bg = 224;

	hNoteBackgroundPen = CreatePen(PS_SOLID, 1, RGB(fg, fg, fg));
	hNoteBackgroundBrush = CreateSolidBrush(RGB(fg, fg, fg));
	hControllerBrush = CreateSolidBrush(RGB(bg, bg, bg));

    hControllerFont = CreateFont(12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Arial");

/*    hJapaneseFont = CreateFont(16, 0, 0, 0, FW_NORMAL, 0, 0, 0, SHIFTJIS_CHARSET, 
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "MS Gothic");*/
}

void free_gdi_resources(void)
{
	DeleteObject(hNoteBackgroundPen);
	DeleteObject(hNoteBackgroundBrush);
	DeleteObject(hControllerBrush);
	DeleteObject(hControllerFont);
//	DeleteObject(hJapaneseFont);
}

// Handles a click or mouse movement in the main window
void handle_mousedown(int x, int y, int button, int msg)
{
	int i;
	static int lbutton_down = 0;
	static int channel_clicked = -1;
	static int channel_was_locked = 0;

	switch (msg)
	{
		case WM_LBUTTONDOWN:
			lbutton_down = 1;
			i = get_clicked_channel(x, y);
			channel_clicked = i;
			if (i != -1)
			{
				// Remember whether or not this channel had its controller locked...
				channel_was_locked = ms.channels[i].lock_controller;
				// Remember this channel...
				handle_controller_bar_click(x, y, i);
			}
			break;

		case WM_LBUTTONUP:
			lbutton_down = 0;
			// If this channel's controller wasn't locked when the user first clicked on it,
			// unlock it now... since we locked it in handle_controller_bar_click()
			if (channel_clicked != -1 && !channel_was_locked)
				ms.channels[channel_clicked].lock_controller = 0;
			break;

		case WM_RBUTTONUP:
			i = get_clicked_channel(x, y);
			if (i != -1)
				handle_controller_bar_popup(x, y, i);
			break;

		case WM_MOUSEMOVE:
			if (lbutton_down && (button & MK_LBUTTON) && channel_clicked != -1)
				handle_controller_bar_click(x, y, channel_clicked);
			break;
	}
}

// Handle a click or drag on a controller/note bar
void handle_controller_bar_click(int x, int y, int channel)
{
	int c, v;

	// Ya?  Woohoo!  Do something!
	if (ms.playing)
	{
		// Get the displayed controller
		c = ms.channels[channel].displayed_controller;
		if (c != -1)
		{
			// Lock the controller value so it doesn't change while the user is playing with it!
			ms.channels[channel].lock_controller = 1;
			// Calculate the new value based on where the mouse is along the bar
			v = (x - BAR_X) * 127 / BAR_WIDTH;
			// Make sure the new value is within bounds
			if (v > 127)
				v = 127;
			if (v < 0)
				v = 0;
			// Store the controller value
			ms.channels[channel].controllers[c] = v;
			// Output the control change
			set_channel_controller(channel, c, v);
			// Update the display for this channel
			ms.channels[channel].drawn = 1;
			update_display(NULL);
		}
	}
}

// Returns the channel # clicked on, if any... if not, returns -1
int get_clicked_channel(int x, int y)
{
	int i;

	// Loop through the channels
	for (i = 0; i < 16; i++)
		// See if the mouse is in this channel bar
		if (x > BAR_X - 1 && x < BAR_X + BAR_WIDTH + 1 && 
			y > BAR_Y + i * BAR_VSPACE && y < BAR_Y + i * BAR_VSPACE + BAR_HEIGHT)
			return i;

	return -1;
}

// Handles the user right-clicking on a controller bar to bring up a popup menu
void handle_controller_bar_popup(int x, int y, int channel)
{
	POINT pt;
	HMENU hmenu, cmenu;
	MENUITEMINFO mii;
	char buf[256];
	int i, v;
	char *ch;
	channel_state_t *c;

	// Grab a pointer to the channel state for this channel for easy reference
	c = &ms.channels[channel];

	// Initialize the MENUITEMINFO struct
	memset(&mii, 0, sizeof(mii));
	mii.cbSize = sizeof(MENUITEMINFO);

	// Create a submenu for ALL controller names
	cmenu = CreatePopupMenu();
	for (i = 0; i < sizeof(controller_names) / sizeof(char *); i++)
	{
		if (strcmp(controller_names[i], "Unknown controller"))
		{
			mii.fMask = MIIM_ID | MIIM_TYPE;
			mii.wID = i + 1024;
			mii.fType = MFT_STRING;
			mii.dwTypeData = controller_names[i];
			InsertMenuItem(cmenu, 999, TRUE, &mii);
		}
	}

	// Create a popup menu
	GetCursorPos(&pt);
	hmenu = CreatePopupMenu();
	// Insert OVERRIDE item
	sprintf(buf, "Override Changes");
	mii.fMask = MIIM_ID | MIIM_TYPE | MIIM_STATE;
	mii.fState = 0;
	if (c->displayed_controller == -1)
		mii.fState |= MFS_GRAYED;
	else
		if (c->controller_overridden[c->displayed_controller])
			mii.fState |= MFS_CHECKED;

	mii.fType = MFT_STRING;
	mii.dwTypeData = buf;
	mii.wID = 1;
	InsertMenuItem(hmenu, 999, TRUE, &mii);
	// Insert separator
	mii.fMask = MIIM_STATE | MIIM_TYPE;
	mii.fState = MFS_ENABLED;
	mii.fType = MFT_SEPARATOR;
	InsertMenuItem(hmenu, 999, TRUE, &mii);
	// Insert last used controller if it's not a common controller and there is one
	if (c->last_controller != -1)
	{
		for (i = 0; i < sizeof(common_controller_names) / sizeof(char *); i++)
			if (atoi(common_controller_names[i]) == c->last_controller)
				break;
		if (i == sizeof(common_controller_names) / sizeof(char *))
		{
			mii.fMask = MIIM_ID | MIIM_TYPE | MIIM_STATE;
			mii.fState = MFS_DEFAULT;
			mii.wID = c->last_controller + 1024;
			mii.fType = MFT_STRING;
			strcpy(buf, controller_names[c->last_controller]);
			mii.dwTypeData = buf;
			InsertMenuItem(hmenu, 999, TRUE, &mii);
		}
	}
	// Insert COMMON controller names
	for (i = 0; i < sizeof(common_controller_names) / sizeof(char *); i++)
	{
		ch = strchr(common_controller_names[i], '-');
		mii.fMask = MIIM_ID | MIIM_TYPE;
		if (atoi(common_controller_names[i]) == c->last_controller)
		{
			mii.fMask |= MIIM_STATE;
			mii.fState = MFS_DEFAULT;
		}
		mii.wID = atoi(common_controller_names[i]) + 1024;
		mii.fType = MFT_STRING;
		mii.dwTypeData = ch + 2;
		InsertMenuItem(hmenu, 999, TRUE, &mii);
	}
	// Insert "All Controllers" submenu
	mii.fMask = MIIM_SUBMENU | MIIM_TYPE;
	mii.fType = MFT_STRING;
	strcpy(buf, "All Controllers");
	mii.dwTypeData = buf;
	mii.hSubMenu = cmenu;
	InsertMenuItem(hmenu, 999, TRUE, &mii);
	// Insert separator
	mii.fMask = MIIM_STATE | MIIM_TYPE;
	mii.fState = MFS_ENABLED;
	mii.fType = MFT_SEPARATOR;
	InsertMenuItem(hmenu, 999, TRUE, &mii);
	// Insert grayed channel ID # text
	mii.fMask = MIIM_STATE | MIIM_TYPE;
	mii.fType = MFT_STRING;
	mii.fState = MFS_ENABLED;
	sprintf(buf, "Channel %d", channel + 1);
	InsertMenuItem(hmenu, 999, TRUE, &mii);
	// Insert controller name and current value IF a controller value is being displayed
	if (c->displayed_controller != -1)
	{
		v = c->controllers[c->displayed_controller];
		if (v != -1)
			sprintf(buf, "%s: %d", controller_names[c->displayed_controller], v);
		else
			sprintf(buf, "%s: [not set]", controller_names[c->displayed_controller]);

		InsertMenuItem(hmenu, 999, TRUE, &mii);
	}

	// Display the menu
	SetForegroundWindow(hwndApp);
	i = TrackPopupMenu(hmenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndApp, NULL);
	DestroyMenu(hmenu);
	if (!i)
		return;

	// Did they choose OVERRIDE?
	if (i == 1)
	{
		if (c->controller_overridden[c->displayed_controller])
		{
			c->controller_overridden[c->displayed_controller] = 0;
		}
		else
		{
			c->controller_overridden[c->displayed_controller] = 1;
			c->lock_controller = 1;
		}
		return;
	}

	// Did they choose a controller?
	if (i >= 1024)
	{
		c->displayed_controller = i - 1024;
		if (c->displayed_controller != c->last_controller)
			c->lock_controller = 1;
		else
			c->lock_controller = 0;
		c->drawn = 1;
		update_display(NULL);
	}
}

// Copies the given string to the clipboard
void copy_to_clipboard(char *str)
{
	HANDLE hClipdata;
	char *clipdata;

	// Allocate enough memory to store the sysex data in text format
	hClipdata = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, strlen(str) + 2);
	// Lock the memory
	clipdata = (char *) GlobalLock(hClipdata);
	if (clipdata)
	{
		// Copy the given string into it
		strcpy(clipdata, str);
		// Unlock it
		GlobalUnlock(hClipdata);
		// Copy it to the clipboard
		if (OpenClipboard(hwndApp))
		{
			EmptyClipboard();
			SetClipboardData(CF_TEXT, hClipdata);
			CloseClipboard();
		}
		// The memory does NOT have to be freed - it's managed by Windows now.
	}
}
// TMIDI include file

#define HINYBBLE(a)		((a) >> 4)
#define LONYBBLE(a)		((a) & 15)
#define MAKEBYTE(l,h)	((l) | ((h) << 4))

#define MAX_MIDI_WAIT	1000.0f	// Max # of milliseconds to sleep in main playback loop
#define MAX_MIDI_TRACKS	65536	// Max # of tracks TMIDI will read from a MIDI file

#define MAX_PITCH_BEND	0x2000	// Max value for pitch bends (this is a MIDI constant, do not change!)

#define MIDI_STANDARD_NONE		1
#define MIDI_STANDARD_GM		2
#define MIDI_STANDARD_GS		4
#define MIDI_STANDARD_XG		8
#define MIDI_STANDARD_MT32		16

#define WMAPP_LOADFILE			(WM_APP + 1)
#define WMAPP_DONE_PLAYING		(WM_APP + 2)
#define WMAPP_REFRESH_CHANNELS	(WM_APP + 3)

// Inter-process communication messages
#define IPC_PLAY	0

// Pitch bar dimensions
//#define BAR_X		170
//#define BAR_Y		55 //79
//#define BAR_WIDTH	93
//#define BAR_HEIGHT	10
//#define BAR_VSPACE	18

#define BAR_X		180 //170
#define BAR_Y		51 //79
#define BAR_WIDTH	93
#define BAR_HEIGHT	15
#define BAR_VSPACE	18

#define MAX_NOTE_HEIGHT		(BAR_HEIGHT - 2)

// Windows variables
HINSTANCE ghInstance = NULL;
HWND hwndApp = NULL;
HWND hwndText = NULL;
HWND hwndStatusBar = NULL;
HKEY key;	// handle to registry key
HHOOK g_hhk;	// hook for tooltips
HWND g_hwndTT;	// window handle for tooltip control
char temp_dir[MAX_PATH] = "";
char analysis_file[MAX_PATH] = "";
char filename_to_load[MAX_PATH] = "";

// GDI resources
HBRUSH hNoteBackgroundBrush = NULL;
HBRUSH hControllerBrush = NULL;
HPEN hNoteBackgroundPen = NULL;
HFONT hControllerFont = NULL;
HFONT hJapaneseFont = NULL;

// Tracks window variables
HWND hwndTracks = NULL;
int tracksLastValuesSet = 0;

// Channels window variables
HWND hwndChannels = NULL;
int channelsLastValuesSet = 0;

// Sysex window variables
HWND hwndSysex = NULL;
int sysexLastValuesSet = 0;

// Settings saved in the registry
int midi_in_cb = 0;
int midi_out_cb = 0;
int appRectSaved = 0;
int textRectSaved = 0;
int tracksRectSaved = 0;
int channelsRectSaved = 0;
int sysexRectSaved = 0;
int genericTextRectSaved = 0;
RECT appRect;
RECT textRect;
RECT tracksRect;
RECT channelsRect;
RECT sysexRect;
RECT genericTextRect;
int alwaysCheckAssociations = 1;

// MIDI I/O handles
HMIDIIN hin = NULL;
HMIDIOUT hout = NULL;

// Timing variables
LARGE_INTEGER LIfreq;
LARGE_INTEGER LIms_time;
int freq;
int hr_ms_time;

typedef struct playlist_t {
	char filename[MAX_PATH];
	struct playlist_t *prev;
	struct playlist_t *next;
} playlist_t;

typedef struct midi_header_t {
	char id[4];							// MIDI header ID ("MThd")
	unsigned int header_size;			// Size of header in bytes
	unsigned short file_format;			// MIDI file type (0, 1, or 2)
	unsigned short num_tracks;			// Number of tracks in MIDI file
	unsigned short num_ticks;			// Number of ticks per quarter note (used to calc tempo)
} midi_header_t;

typedef struct track_header_t {
	char id[4];							// Track header ID ("MTrk")
	unsigned int length;				// Length of track chunk in bytes
	unsigned int num_events;			// Number of events in this track
	unsigned int num_events_done;		// Number of events processed in this track
	unsigned char *data;				// Pointer to beginning of track chunk
	unsigned char *dataptr;				// Pointer to current location in track chunk
	double start;						// Starting time of current event
	double trigger;						// Ending time of current event
	double dt;							// Length of current event in ticks
	char enabled;						// Is this track enabled?
	unsigned short tracknum;			// Number of this track
	unsigned char lastcmd;				// Last command byte (used in running mode)
	// Historical values
	char name[256];						// Name of this track
	signed char last_channel;			// Last channel used by this track
	signed char last_note_pitch;		// Last note pitch played from this track
	signed char last_note_velocity;		// Last note velocity played from this track
	signed char last_controller;		// Last controller set from this track
	signed char last_controller_value;	// Last controller value set from this track
	signed char last_program;			// Last program set from this track
	signed char last_bank;				// Last bank set from this track
	signed int last_pitch_bend;			// Last pitch bend set from this track
} track_header_t;

typedef struct channel_state_t {
	// State values
	int normal_program;					// Normal program used by this channel
	int override_program;				// Program that the user has overridden this channel with
	unsigned char notes[128];			// Current velocity for each possible note (0 is off)
	signed short int controllers[128];	// Continuous controller values (-1 is unset)
	unsigned char controller_overridden[128];	// Is this controller overridden with a user-specified value?
	int note_count;						// Number of notes currently on
	int displayed_controller;			// Currently displayed controller (over background for note display)
	unsigned char lock_controller;		// Prevent automatically switching displayed controller to last used?
	// State flags
	int program_overridden;				// Has the user overridden this channel's program?
	int muted;							// Is this channel muted?
	int drawn;							// Have lines been drawn for this channel since last frame?
	// Historical values
	signed char track_references[MAX_MIDI_TRACKS];	// Tracks which have events on this channel
	signed char last_note_pitch;		// Last note pitch played from this track
	signed char last_note_velocity;		// Last note velocity played from this track
	signed char last_controller;		// Last controller set from this track
	signed char last_controller_value;	// Last controller value set from this track
	signed char last_program;			// Last program set from this track
	signed char last_bank;				// Last bank set from this track
	signed int last_pitch_bend;			// Last pitch bend set from this track
} channel_state_t;

typedef struct midi_state_t {
	char perform_analysis;				// Should this song be analyzed on loading?
	char analyzing;						// Is this song currently being analyzed?
	char seek_sliding;					// Is the seek bar sliding? (user holding down button)
	char seeking;						// Is this song currently seeking?
	double seek_to;						// Place to seek to (in milliseconds)
	int stop_requested;					// Has there been a request to stop the playback thread?
	int playing;						// Is the playback thread playing?
	int paused;							// Is the playback thread paused?
	int found_note_on;					// Has there been a Note On event yet?
	double first_note_on;				// Time when the first Note On event was found
	int finished_naturally;				// Did the current MIDI file finish naturally?
	int loop_count;						// Number of times left to loop the playback
	double starttime;					// Starting playback time
	double curtime;						// Current playback time
	unsigned int tempo;					// Current tempo
	double tick_length;					// Current tick length in milliseconds
	int song_length;					// Length of song in seconds
	int num_events;						// Number of MIDI events in song
	int peak_polyphony;					// Peak polyphony seen throughout the song
	int uses_percussion;				// Does this MIDI file use percussion? (channel 10)
	int highest_pitch_bend;				// Highest pitch bend used in song (used in pitch bend visualization calculations)
	signed int mod_velocity;			// Velocity modification (-127 to 127)
	signed int mod_pitch;				// Pitch modification (-127 to 127)
	char current_text[256];				// Current text event being displayed
	int title_displayed;				// Has the title been displayed yet?
	channel_state_t channels[16];		// Channel state structs
	int midi_standard;					// MIDI standard used by this MIDI file (as sysex indicates)
	char filename[256];
} midi_state_t;

typedef struct midi_text_t {
	char *midi_text;					// MIDI text string
	int text_type;						// Type of text event
	double midi_time;					// Time (in seconds) when this text event occurs
	int track;							// Track this text is embedded in
	int track_offset;					// Offset (in bytes) where this text event occurs

	midi_text_t *next;
} midi_text_t;

typedef struct midi_sysex_t {
	unsigned char *data;				// Pointer to sysex data (in track chunk in memory)
	int length;							// Length of this sysex
	double midi_time;					// Time (in seconds) when this sysex occurs
	int track;							// Track this sysex is embedded in
	int track_offset;					// Offset (in bytes) where this sysex occurs
	int channel;						// Channel this sysex data goes out on

	midi_sysex_t *next;
} midi_sysex_t;

typedef struct midi_device_t {
	char *user_device_name;				// User-defined name for this device
	char enabled;						// Is this device currently enabled for playback?
	char usable;						// Does the user consider this device usable?
	int standards;						// Device type flags (MT-32, GM, GS, XG)
	char dx_device;						// Is this a DirectMusic device?
	char input_device;					// Is this an input device?
	MIDIINCAPS incaps;					// Input device caps
	MIDIOUTCAPS outcaps;				// Output device caps

	midi_device_t *next;
} midi_device_t;

midi_device_t *midi_devices = NULL;		// MIDI devices
playlist_t *playlist = NULL;			// Playlist
playlist_t *playback_head = NULL;		// Pointer to playlist entry currently being played (or last played)
midi_header_t mh;						// MIDI file header struct
track_header_t *th = NULL;				// Track state structs
midi_state_t ms;						// MIDI state struct
midi_text_t *midi_text_events = NULL;	// MIDI text events
midi_sysex_t *midi_sysex_events = NULL;	// MIDI sysex events
char mt32_patch_groups[128];			// MT-32 timbre group for a given patch
char mt32_patch_programs[128];			// MT-32 program number for a given patch
char mt32_memory_names[64][11];			// MT-32 memory timbre names

char *sysex_manufacturer_names[] = {
"126 - GM", 
"127 - (All)", 
"125 - (Private Use)", 
"64 - Kawai",
"65 - Roland",
"66 - Korg", 
"67 - Yamaha",
"68 - Casio",
"70 - Kamiya Studio",
"71 - Akai",
"72 - Victor",
"75 - Fujitsu",
"76 - Sony",
"78 - Teac",
"80 - Matsushita",
"81 - Fostex",
"82 - Zoom",
"84 - Matsushita",
"85 - Suzuki",
"86 - Fuji",
"87 - Acoustic Technical Laboratory"
};

char *drum_kit_names[] = {

"0 - Standard",
"1 - Standard 2",
"2 - Standard L/R",
"8 - Room",
"9 - Hip Hop",
"10 - Jungle",
"11 - Techno",
"12 - Room L/R",
"13 - House",
"16 - Power",
"24 - Electronic",
"25 - TR-808",
"26 - Dance",
"27 - CR-78",
"28 - TR-606",
"29 - TR-707",
"30 - TR-909",
"32 - Jazz",
"33 - Jazz L/R",
"40 - Brush",
"41 - Brush 2",
"42 - Brush 2 L/R",
"48 - Orchestra",
"49 - Ethnic",
"50 - Kick && Snare",
"51 - Kick && Snare 2",
"52 - Asia",
"53 - Cymbal && Claps",
"54 - Gamelan",
"55 - Gamelan 2",
"56 - SFX",
"57 - Rhythm FX",
"58 - Rhythm FX 2",
"59 - Rhythm FX 3",
"60 - SFX 2",
"61 - Voice",
"62 - Cymbal && Claps 2",
"127 - CM-64/32"

};

/*char *drum_kit_names[] = {

"0 - Standard",
"1 - Standard 2",
"7 - Room",
"8 - Room 2",
"16 - Power",
"24 - Electronic",
"25 - TR-808",
"26 - Dance",
"30 - TR-909",
"32 - Jazz",
"40 - Brush",
"48 - Orchestra",
"49 - Ethnic",
"50 - Kick & Snare",
"56 - SFX",
"57 - Rhythm FX",
"127 - CM-64/32"

};*/

/*char *drum_kit_names[] = {

"0 - Standard Kit", 
"1 - Standard Kit 2",
"3 - Brite Kit",
"4 - Slim Kit",
"8 - Room Kit",
"9 - Dark Room Kit",
"16 - Rock Kit",
"17 - Slam Kit",
"24 - Electronic Kit",
"25 - Analog Kit",
"27 - Dance Kit",
"28 - HipHop Kit",
"29 - Jungle Kit",
"32 - Jazz Kit",
"40 - Brush Kit",
"48 - Orchestra Kit"

};*/

char *gm_program_names[] = {

"Acoustic Grand Piano",
"Bright Acoustic Piano",
"Electric Grand Piano",
"Honky-tonk Piano",
"Electric Piano 1",
"Electric Piano 2",
"Harpsichord",
"Clavi",
"Celesta",
"Glockenspiel",
"Music Box",
"Vibraphone",
"Marimba",
"Xylophone",
"Tubular Bells",
"Dulcimer",
"Drawbar Organ",
"Percussive Organ",
"Rock Organ",
"Church Organ",
"Reed Organ",
"Accordion",
"Harmonica",
"Tango Accordion",
"Acoustic Guitar (nylon)",
"Acoustic Guitar (steel)",
"Electric Guitar (jazz)",
"Electric Guitar (clean)",
"Electric Guitar (muted)",
"Overdriven Guitar",
"Distortion Guitar",
"Guitar harmonics",
"Acoustic Bass",
"Electric Bass (finger)",
"Electric Bass (pick)",
"Fretless Bass",
"Slap Bass 1",
"Slap Bass 2",
"Synth Bass 1",
"Synth Bass 2",
"Violin",
"Viola",
"Cello",
"Contrabass",
"Tremolo Strings",
"Pizzicato Strings",
"Orchestral Harp",
"Timpani",
"String Ensemble 1",
"String Ensemble 2",
"SynthStrings 1",
"SynthStrings 2",
"Choir Aahs",
"Voice Oohs",
"Synth Voice",
"Orchestra Hit",
"Trumpet",
"Trombone",
"Tuba",
"Muted Trumpet",
"French Horn",
"Brass Section",
"SynthBrass 1",
"SynthBrass 2",
"Soprano Sax",
"Alto Sax",
"Tenor Sax",
"Baritone Sax",
"Oboe",
"English Horn",
"Bassoon",
"Clarinet",
"Piccolo",
"Flute",
"Recorder",
"Pan Flute",
"Blown Bottle",
"Shakuhachi",
"Whistle",
"Ocarina",
"Lead 1 (square)",
"Lead 2 (sawtooth)",
"Lead 3 (calliope)",
"Lead 4 (chiff)",
"Lead 5 (charang)",
"Lead 6 (voice)",
"Lead 7 (fifths)",
"Lead 8 (bass + lead)",
"Pad 1 (new age)",
"Pad 2 (warm)",
"Pad 3 (polysynth)",
"Pad 4 (choir)",
"Pad 5 (bowed)",
"Pad 6 (metallic)",
"Pad 7 (halo)",
"Pad 8 (sweep)",
"FX 1 (rain)",
"FX 2 (soundtrack)",
"FX 3 (crystal)",
"FX 4 (atmosphere)",
"FX 5 (brightness)",
"FX 6 (goblins)",
"FX 7 (echoes)",
"FX 8 (sci-fi)",
"Sitar",
"Banjo",
"Shamisen",
"Koto",
"Kalimba",
"Bag pipe",
"Fiddle",
"Shanai",
"Tinkle Bell",
"Agogo",
"Steel Drums",
"Woodblock",
"Taiko Drum",
"Melodic Tom",
"Synth Drum",
"Reverse Cymbal",
"Guitar Fret Noise",
"Breath Noise",
"Seashore",
"Bird Tweet",
"Telephone Ring",
"Helicopter",
"Applause",
"Gunshot"

};

char **program_names = gm_program_names;

char *program_categories[] = {

"Piano",
"Chromatic Percussion",
"Organ",
"Guitar",
"Bass",
"Strings",
"Ensemble",
"Brass",
"Reed",
"Pipe",
"Synth Lead",
"Synth Pad",
"Synth Effects",
"Ethnic",
"Percussive",
"Sound Effects"

};

char *mt32_program_names[] = {

"Acou Piano 1", 
"Acou Piano 2", 
"Acou Piano 3", 
"Elec Piano 1", 
"Elec Piano 2", 
"Elec Piano 3", 
"Elec Piano 4", 
"Honkytonk", 
"Elec Org 1", 
"Elec Org 2", 
"Elec Org 3", 
"Elec Org 4", 
"Pipe Org 1", 
"Pipe Org 2", 
"Pipe Org 3", 
"Accordion", 
"Harpsi 1", 
"Harpsi 2", 
"Harpsi 3", 
"Clavi 1", 
"Clavi 2", 
"Clavi 3", 
"Celesta 1", 
"Celesta 2", 
"Syn Brass 1", 
"Syn Brass 2", 
"Syn Brass 3", 
"Syn Brass 4", 
"Syn Bass 1", 
"Syn Bass 2", 
"Syn Bass 3", 
"Syn Bass 4", 
"Fantasy", 
"Harmo Pan", 
"Chorale", 
"Glasses", 
"Soundtrack", 
"Atmosphere", 
"Warm Bell", 
"Funny Vox", 
"Echo Bell", 
"Ice Rain", 
"Oboe 2001", 
"Echo Pan", 
"Doctor Solo", 
"Schooldaze", 
"Bellsinger", 
"Square Wave", 
"Str Sect 1", 
"Str Sect 2", 
"Str Sect 3", 
"Pizzicato", 
"Violin 1", 
"Violin 2", 
"Cello 1", 
"Cello 2", 
"Contrabass", 
"Harp 1", 
"Harp 2", 
"Guitar 1", 
"Guitar 2", 
"Elec Gtr 1", 
"Elec Gtr 2", 
"Sitar", 
"Acou Bass 1", 
"Acou Bass 2", 
"Elec Bass 1", 
"Elec Bass 2", 
"Slap Bass 1", 
"Slap Bass 2", 
"Fretless 1", 
"Fretless 2", 
"Flute 1", 
"Flute 2", 
"Piccolo 1", 
"Piccolo 2", 
"Recorder", 
"Pan Pipes", 
"Sax 1", 
"Sax 2", 
"Sax 3", 
"Sax 4", 
"Clarinet 1", 
"Clarinet 2", 
"Oboe", 
"Engl Horn", 
"Bassoon", 
"Harmonica", 
"Trumpet 1", 
"Trumpet 2", 
"Trombone 1", 
"Trombone 2", 
"Fr Horn 1", 
"Fr Horn 2", 
"Tuba", 
"Brs Sect 1", 
"Brs Sect 2", 
"Vibe 1", 
"Vibe 2", 
"Syn Mallet", 
"Windbell", 
"Glock", 
"Tube Bell", 
"Xylophone", 
"Marimba", 
"Koto", 
"Sho", 
"Shakuhachi", 
"Whistle 1", 
"Whistle 2", 
"Bottleblow", 
"Breathpipe", 
"Timpani", 
"Melodic Tom", 
"Deep Snare", 
"Elec Perc 1", 
"Elec Perc 2", 
"Taiko", 
"Taiko   Rim", 
"Cymbal", 
"Castanets", 
"Triangle", 
"Orche Hit", 
"Telephone", 
"Bird Tweet", 
"One Note Jam", 
"Water Bells", 
"Jungle Tune"

};

char *midi_text_event_descriptions[] = {

"Text",
"Text",
"Copyright text",
"Sequence or track name",
"Track instrument name",
"Lyric",
"Marker",
"Cue point"

};

char *controller_names[] = {

"Bank Select",
"Modulation Wheel",
"Breath controller",
"Unknown controller",
"Foot Pedal",
"Portamento Time",
"Data Entry",
"Volume",
"Balance",
"Unknown controller",
"Pan position",
"Expression",
"Effect Control 1",
"Effect Control 2",
"Unknown controller",
"Unknown controller",
"General Purpose Slider 1",
"General Purpose Slider 2",
"General Purpose Slider 3",
"General Purpose Slider 4",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Bank Select (fine)",
"Modulation Wheel (fine)",
"Breath controller (fine)",
"Unknown controller",
"Foot Pedal (fine)",
"Portamento Time (fine)",
"Data Entry (fine)",
"Volume (fine)",
"Balance (fine)",
"Unknown controller",
"Pan position (fine)",
"Expression (fine)",
"Effect Control 1 (fine)",
"Effect Control 2 (fine)",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Hold Pedal (on/off)",
"Portamento (on/off)",
"Sustenuto Pedal (on/off)",
"Soft Pedal (on/off)",
"Legato Pedal (on/off)",
"Hold 2 Pedal (on/off)",
"Sound Variation",
"Sound Timbre",
"Sound Release Time",
"Sound Attack Time",
"Sound Brightness",
"Sound Decay Time",
"Sound Vibrato Rate",
"Sound Vibrato Depth",
"Sound Vibrato Delay",
"Sound Control 10",
"General Purpose Button 1 (on/off)",
"General Purpose Button 2 (on/off)",
"General Purpose Button 3 (on/off)",
"General Purpose Button 4 (on/off)",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Effects Level",
"Tremolo Level",
"Chorus Level",
"Celeste Level",
"Phaser Level",
"Data Button increment",
"Data Button decrement",
"Non-registered Parameter (fine)",
"Non-registered Parameter (coarse)",
"Registered Parameter (fine)",
"Registered Parameter (coarse)",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"Unknown controller",
"All Sound Off",
"All Controllers Off",
"Local Keyboard (on/off)",
"All Notes Off",
"Omni Mode Off",
"Omni Mode On",
"Mono Operation",
"Poly Operation"

};

char *common_controller_names[] = {

"7 - Volume",
"11 - Expression",
"10 - Pan position",
"1 - Modulation Wheel",
"64 - Hold Pedal (on/off)",
"91 - Effects Level",
"93 - Chorus Level"

};

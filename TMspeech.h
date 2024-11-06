// Tom's E-Mail Client, started on 9/21/98
// By Tom Grandgent (tgrand@canvaslink.com)

// Speech functions

int init_tts_engine(void);
void speak(char *text, ...);
void stop_speaking(void);
int shutdown_tts_engine(void);
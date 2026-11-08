#ifndef APP_CONSOLE_H
#define APP_CONSOLE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the interactive serial console (REPL) and register all commands.
 */
void app_console_start(void);

#ifdef __cplusplus
}
#endif

#endif // APP_CONSOLE_H

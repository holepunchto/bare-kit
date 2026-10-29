#ifndef BARE_KIT_SYMBOLS_H
#define BARE_KIT_SYMBOLS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Make the symbols of Bare visible to the addons of the process, regardless of
 * how the library was loaded.
 */
void
bare_kit__expose_symbols(void);

#ifdef __cplusplus
}
#endif

#endif // BARE_KIT_SYMBOLS_H

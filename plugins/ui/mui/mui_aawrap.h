/* Selecta's wrappers around the UP- classes (design 8.3): the mui_aa<class>_new constructors are declared in the class headers
 * (the spec structs live there); this frees what the wrappers own (the list hooks). Call it after the application is disposed. */
#ifndef MUI_AAWRAP_H
#define MUI_AAWRAP_H

void mui_aawrap_free(void);

#endif

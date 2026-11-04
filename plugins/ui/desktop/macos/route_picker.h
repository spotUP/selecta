/* I1039: AVRoutePickerView hosted in the SDL window's content view (route_picker.m). Plain C interface for backend_sdl.c. */
#ifndef SELECTA_DESKTOP_ROUTE_PICKER_H
#define SELECTA_DESKTOP_ROUTE_PICKER_H

void* sel_route_picker_new(void);
void sel_route_picker_free(void* picker);
/* Window points from the top left of the content view; show = 0 hides (deferred while the route menu is open). */
void sel_route_picker_place(void* picker, void* nswindow, double x, double y, double w, double h, int show);
/* Times macOS began presenting the route menu (the delegate's routePickerViewWillBeginPresentingRoutes). */
unsigned sel_route_picker_presents(void* picker);
int sel_route_picker_visible(void* picker);

#endif

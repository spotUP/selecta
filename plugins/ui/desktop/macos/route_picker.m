/* I1039: the system AirPlay picker inside the SDL window. Selecta's UI is custom-drawn, and macOS has no public call that
 * opens the AirPlay route menu programmatically, so a real AVRoutePickerView (button colours clear, no border) sits over
 * the "AirPlay Speakers" row while the device menu is open; the click lands on it and macOS shows its own route menu.
 * The picker has no player, so the route it picks is the app's own route, which reaches the AVSampleBufferAudioRenderer
 * of the CoreAudio output (proven on a HomePod, 2026-11-03) and never the HAL unit. The delegate counts presentations;
 * the UI turns a new one into the route pick (PREF_SET output.device selecta:airplay-route). Compiled with ARC. */
#include "route_picker.h"

#import <AVKit/AVKit.h>
#import <AppKit/AppKit.h>

@interface SelRoutePicker : NSObject <AVRoutePickerViewDelegate>
@property(nonatomic, strong) AVRoutePickerView* view;
@property(nonatomic) unsigned presents; /* routePickerViewWillBeginPresentingRoutes calls */
@property(nonatomic) BOOL presenting;  /* the route menu is open: the anchor view must stay */
@property(nonatomic) BOOL hide_after;  /* the UI asked to hide while the menu was open */
@end

@implementation SelRoutePicker
- (void)routePickerViewWillBeginPresentingRoutes:(AVRoutePickerView*)v {
    (void)v;
    self.presents++;
    self.presenting = YES;
}
- (void)routePickerViewDidEndPresentingRoutes:(AVRoutePickerView*)v {
    self.presenting = NO;
    if (self.hide_after) {
        v.hidden = YES;
        self.hide_after = NO;
    }
}
@end

void* sel_route_picker_new(void) {
    @autoreleasepool {
        SelRoutePicker* p = [[SelRoutePicker alloc] init];
        AVRoutePickerView* v = [[AVRoutePickerView alloc] initWithFrame:NSMakeRect(0, 0, 16, 16)];
        NSInteger st;
        v.routePickerButtonBordered = NO;
        for (st = AVRoutePickerViewButtonStateNormal; st <= AVRoutePickerViewButtonStateActiveHighlighted; st++) {
            [v setRoutePickerButtonColor:[NSColor clearColor] forState:(AVRoutePickerViewButtonState)st];
        }
        v.delegate = p;
        v.hidden = YES;
        p.view = v;
        return (__bridge_retained void*)p;
    }
}

void sel_route_picker_free(void* picker) {
    @autoreleasepool {
        SelRoutePicker* p = (__bridge_transfer SelRoutePicker*)picker;
        p.view.delegate = nil;
        [p.view removeFromSuperview];
        p = nil;
    }
}

/* x, y, w, h in window points from the top left. show = 0 hides the view, or defers the hide until the route menu closes
 * (removing the anchor would close the menu under the user's pointer). */
void sel_route_picker_place(void* picker, void* nswindow, double x, double y, double w, double h, int show) {
    @autoreleasepool {
        SelRoutePicker* p = (__bridge SelRoutePicker*)picker;
        NSWindow* win = (__bridge NSWindow*)nswindow;
        NSView* host = win.contentView;
        if (!show) {
            if (p.presenting) {
                p.hide_after = YES;
            } else {
                p.view.hidden = YES;
            }
            return;
        }
        p.hide_after = NO;
        if (host == nil) {
            return;
        }
        if (p.view.superview != host) {
            [host addSubview:p.view positioned:NSWindowAbove relativeTo:nil];
        }
        p.view.frame = NSMakeRect(x, host.isFlipped ? y : host.bounds.size.height - y - h, w, h);
        p.view.hidden = NO;
    }
}

unsigned sel_route_picker_presents(void* picker) {
    SelRoutePicker* p = (__bridge SelRoutePicker*)picker;
    return p.presents;
}

int sel_route_picker_visible(void* picker) {
    SelRoutePicker* p = (__bridge SelRoutePicker*)picker;
    return p.view.superview != nil && !p.view.hidden;
}

#import <UIKit/UIKit.h>
#import <QuartzCore/CAMetalLayer.h>
#include <memory>
#include <stdexcept>
#include "konbini/app/native_mobile_runtime.h"
#include "pictor/surface/ios_surface_provider.h"
@interface KonbiniView : UIView
@property(nonatomic,assign) konbini::app::NativeMobileRuntime* game;
@end
@implementation KonbiniView
+ (Class)layerClass { return [CAMetalLayer class]; }
- (void)sendTouches:(NSSet<UITouch*>*)touches phase:(konbini::app::TouchPhase)phase {
    if(!self.game) return;
    try { for(UITouch* touch in touches) {
        const CGPoint point=[touch locationInView:self];
        self.game->touch(reinterpret_cast<std::uintptr_t>((__bridge void*)touch),phase,
            point.x*self.contentScaleFactor,point.y*self.contentScaleFactor);
    }} catch(const std::exception& error) {
        self.game->pause(true);
        NSLog(@"[konbini] touch input failed: %s",error.what());
    }
}
- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event { [self sendTouches:touches phase:konbini::app::TouchPhase::Down]; }
- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event { [self sendTouches:touches phase:konbini::app::TouchPhase::Move]; }
- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event { [self sendTouches:touches phase:konbini::app::TouchPhase::Up]; }
- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event { [self sendTouches:touches phase:konbini::app::TouchPhase::Cancel]; }
@end
@interface KonbiniController : UIViewController {
    std::unique_ptr<konbini::app::NativeMobileRuntime> _game;
    std::unique_ptr<pictor::IOSSurfaceProvider> _surface;
}
@property(nonatomic,strong) KonbiniView* canvas;
@property(nonatomic,strong) CADisplayLink* displayLink;
@property(nonatomic,assign) BOOL failed;
@end
@implementation KonbiniController
- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor=UIColor.blackColor;
    self.canvas=[[KonbiniView alloc] initWithFrame:CGRectZero];
    self.canvas.multipleTouchEnabled=YES;
    [self.view addSubview:self.canvas];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(suspend) name:UIApplicationWillResignActiveNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(resume) name:UIApplicationDidBecomeActiveNotification object:nil];
}
- (UIInterfaceOrientationMask)supportedInterfaceOrientations { return UIInterfaceOrientationMaskLandscape; }
- (BOOL)prefersStatusBarHidden { return YES; }
- (void)showError:(const char*)message {
    self.failed=YES;[self suspend];
    UILabel* label=[[UILabel alloc] initWithFrame:self.view.safeAreaLayoutGuide.layoutFrame];
    label.numberOfLines=0;label.textColor=UIColor.whiteColor;
    label.text=[NSString stringWithUTF8String:message];
    [self.view addSubview:label];
    NSLog(@"[konbini] %@",label.text);
}
- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    if(self.failed) return;
    self.canvas.frame=self.view.safeAreaLayoutGuide.layoutFrame;
    self.canvas.contentScaleFactor=self.view.window.screen.scale ?: UIScreen.mainScreen.scale;
    CAMetalLayer* layer=(CAMetalLayer*)self.canvas.layer;
    const double scale=self.canvas.contentScaleFactor;
    const auto width=static_cast<uint32_t>(self.canvas.bounds.size.width*scale);
    const auto height=static_cast<uint32_t>(self.canvas.bounds.size.height*scale);
    if(!width || !height) return;
    layer.drawableSize=CGSizeMake(width,height);
    try {
        if(!_game) {
            _game=std::make_unique<konbini::app::NativeMobileRuntime>(NSBundle.mainBundle.resourcePath.UTF8String);
            _surface=std::make_unique<pictor::IOSSurfaceProvider>((__bridge void*)layer,width,height);
            _game->attach(*_surface,{width,height},scale);
            self.canvas.game=_game.get();
            [self resume];
        } else {
            _surface->update_layer((__bridge void*)layer,width,height);
            _game->resize({width,height},scale);
        }
    } catch(const std::exception& error) {[self showError:error.what()];}
}
- (void)frame:(CADisplayLink*)link {
    if(!_game || self.failed) return;
    try {_game->frame(link.timestamp);}
    catch(const std::exception& error) {[self showError:error.what()];}
}
- (void)suspend {
    [self.displayLink invalidate];self.displayLink=nil;
    if(_game) _game->pause(true);
}
- (void)resume {
    if(!_game || self.failed || self.displayLink || UIApplication.sharedApplication.applicationState!=UIApplicationStateActive) return;
    _game->pause(false);
    self.displayLink=[CADisplayLink displayLinkWithTarget:self selector:@selector(frame:)];
    [self.displayLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
}
- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [self.displayLink invalidate];self.canvas.game=nullptr;
    if(_game) _game->detach();
}
@end
@interface KonbiniDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic,strong) UIWindow* window;
@end
@implementation KonbiniDelegate
- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)options {
    self.window=[[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    self.window.rootViewController=[KonbiniController new];
    [self.window makeKeyAndVisible];return YES;
}
@end
int main(int argc,char** argv) {
    @autoreleasepool {return UIApplicationMain(argc,argv,nil,NSStringFromClass(KonbiniDelegate.class));}
}

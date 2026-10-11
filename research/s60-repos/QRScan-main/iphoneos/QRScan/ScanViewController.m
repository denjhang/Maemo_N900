#import <UIKit/UIKit.h>
#import <AVFoundation/AVFoundation.h>
#import "ResultViewController.h"
#import "QRDecoder.h"

@interface ScanViewController : UIViewController
    <UINavigationControllerDelegate,
     UIImagePickerControllerDelegate>
@end

@implementation ScanViewController {
    AVCaptureSession           *_session;
    AVCaptureVideoPreviewLayer *_previewLayer;
    AVCaptureMetadataOutput    *_metaOutput;
    UIButton                   *_pickBtn;
    UIButton                   *_flashBtn;
    UILabel                    *_hintLabel;
    BOOL                        _hasCamera;
    BOOL                        _torchOn;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.title = @"QRScan";
    self.view.backgroundColor = [UIColor blackColor];

    _hasCamera = [UIImagePickerController
        isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera];

    [self buildUI];

    if (_hasCamera) {
        Class captureClass = NSClassFromString(@"AVCaptureSession");
        if (captureClass) {
            [self setupLiveCamera];
        }
    }
}

- (void)buildUI {
    CGRect bounds = self.view.bounds;
    CGFloat bW    = 180.0f;
    CGFloat bH    = 44.0f;

    _hintLabel = [[UILabel alloc] initWithFrame:CGRectMake(0, bounds.size.height - 140, bounds.size.width, 40)];
    _hintLabel.text          = _hasCamera ? @"Point camera at a QR code" : @"Pick an image to scan";
    _hintLabel.textAlignment = NSTextAlignmentCenter;
    _hintLabel.textColor     = [UIColor whiteColor];
    _hintLabel.backgroundColor = [UIColor clearColor];
    _hintLabel.font          = [UIFont systemFontOfSize:14.0f];
    [self.view addSubview:_hintLabel];

    _pickBtn = [UIButton buttonWithType:UIButtonTypeRoundedRect];
    _pickBtn.frame = CGRectMake((bounds.size.width - bW) / 2, bounds.size.height - 90, bW, bH);
    [_pickBtn setTitle:@"Pick from Library" forState:UIControlStateNormal];
    [_pickBtn addTarget:self action:@selector(pickFromLibrary) forControlEvents:UIControlEventTouchUpInside];
    [self.view addSubview:_pickBtn];

    if (_hasCamera) {
        _flashBtn = [UIButton buttonWithType:UIButtonTypeRoundedRect];
        _flashBtn.frame = CGRectMake(bounds.size.width - 80, 10, 70, 36);
        [_flashBtn setTitle:@"Flash" forState:UIControlStateNormal];
        [_flashBtn addTarget:self action:@selector(toggleFlash) forControlEvents:UIControlEventTouchUpInside];
        [self.view addSubview:_flashBtn];
    }

    UIButton *historyBtn = [UIButton buttonWithType:UIButtonTypeRoundedRect];
    historyBtn.frame = CGRectMake(10, 10, 70, 36);
    [historyBtn setTitle:@"History" forState:UIControlStateNormal];
    [historyBtn addTarget:self action:@selector(openHistory) forControlEvents:UIControlEventTouchUpInside];
    [self.view addSubview:historyBtn];
}

- (void)setupLiveCamera {
    _session = [[AVCaptureSession alloc] init];
    _session.sessionPreset = AVCaptureSessionPresetHigh;

    AVCaptureDevice *device = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
    if (!device) return;

    NSError *error = nil;
    AVCaptureDeviceInput *input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&error];
    if (error || !input) return;

    if ([_session canAddInput:input]) [_session addInput:input];

    _metaOutput = [[AVCaptureMetadataOutput alloc] init];
    if ([_session canAddOutput:_metaOutput]) {
        [_session addOutput:_metaOutput];
        [_metaOutput setMetadataObjectsDelegate:self queue:dispatch_get_main_queue()];
        NSArray *types = @[AVMetadataObjectTypeQRCode];
        _metaOutput.metadataObjectTypes = types;
    }

    _previewLayer = [[AVCaptureVideoPreviewLayer alloc] initWithSession:_session];
    _previewLayer.frame = self.view.bounds;
    _previewLayer.videoGravity = AVLayerVideoGravityResizeAspectFill;
    [self.view.layer insertSublayer:_previewLayer atIndex:0];

    [_session startRunning];
}

- (void)captureOutput:(AVCaptureOutput *)output
    didOutputMetadataObjects:(NSArray *)metaObjects
    fromConnection:(AVCaptureConnection *)connection {
    if (metaObjects.count == 0) return;
    AVMetadataMachineReadableCodeObject *obj = metaObjects[0];
    if (!obj.stringValue) return;
    [_session stopRunning];
    [self pushResultWithText:obj.stringValue format:@"QR_CODE"];
}

- (void)pickFromLibrary {
    UIImagePickerController *picker = [[UIImagePickerController alloc] init];
    picker.sourceType = UIImagePickerControllerSourceTypePhotoLibrary;
    picker.delegate   = self;
    [self presentModalViewController:picker animated:YES];
    [picker release];
}

- (void)imagePickerController:(UIImagePickerController *)picker
    didFinishPickingImage:(UIImage *)image editingInfo:(NSDictionary *)editingInfo {
    [self dismissModalViewControllerAnimated:YES];
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSString *result = [QRDecoder decodeImage:image];
        dispatch_async(dispatch_get_main_queue(), ^{
            if (result) {
                [self pushResultWithText:result format:@"QR_CODE"];
            } else {
                UIAlertView *alert = [[UIAlertView alloc]
                    initWithTitle:@"Not found"
                    message:@"No QR code detected in this image."
                    delegate:nil
                    cancelButtonTitle:@"OK"
                    otherButtonTitles:nil];
                [alert show];
                [alert release];
            }
        });
    });
}

- (void)imagePickerControllerDidCancel:(UIImagePickerController *)picker {
    [self dismissModalViewControllerAnimated:YES];
}

- (void)toggleFlash {
    AVCaptureDevice *device = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
    if (!device || ![device hasTorch]) return;
    [device lockForConfiguration:nil];
    _torchOn = !_torchOn;
    device.torchMode = _torchOn ? AVCaptureTorchModeOn : AVCaptureTorchModeOff;
    [device unlockForConfiguration];
    [_flashBtn setTitle:_torchOn ? @"Flash OFF" : @"Flash ON" forState:UIControlStateNormal];
}

- (void)openHistory {
}

- (void)pushResultWithText:(NSString *)text format:(NSString *)format {
    ResultViewController *vc = [[ResultViewController alloc]
        initWithText:text format:format];
    [self.navigationController pushViewController:vc animated:YES];
    [vc release];
}

- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
    if (_session && ![_session isRunning]) [_session startRunning];
}

- (void)viewWillDisappear:(BOOL)animated {
    [super viewWillDisappear:animated];
    if (_session && [_session isRunning]) [_session stopRunning];
}

- (void)dealloc {
    [_session release];
    [_previewLayer release];
    [_metaOutput release];
    [_hintLabel release];
    [super dealloc];
}

@end

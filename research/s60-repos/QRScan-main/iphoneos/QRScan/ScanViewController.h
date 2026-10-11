#import <UIKit/UIKit.h>
#import <AVFoundation/AVFoundation.h>
#import "ResultViewController.h"
#import "QRDecoder.h"

@interface ScanViewController : UIViewController
    <UINavigationControllerDelegate,
     UIImagePickerControllerDelegate,
     AVCaptureMetadataOutputObjectsDelegate>
@end

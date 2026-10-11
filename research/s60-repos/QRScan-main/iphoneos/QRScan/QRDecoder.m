#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <CoreImage/CoreImage.h>

@interface QRDecoder : NSObject
+ (NSString *)decodeImage:(UIImage *)image;
@end

@implementation QRDecoder

+ (NSString *)decodeImage:(UIImage *)image {
    if (!image) return nil;

    CIImage *ciImage = [CIImage imageWithCGImage:image.CGImage];
    if (!ciImage) return nil;

    CIContext *context = [CIContext contextWithOptions:nil];
    NSDictionary *opts = @{ CIDetectorAccuracy: CIDetectorAccuracyHigh };
    CIDetector *detector = [CIDetector detectorOfType:CIDetectorTypeQRCode
                                              context:context
                                              options:opts];
    if (!detector) return nil;

    NSArray *features = [detector featuresInImage:ciImage];
    for (CIFeature *feature in features) {
        if ([feature isKindOfClass:[CIQRCodeFeature class]]) {
            CIQRCodeFeature *qr = (CIQRCodeFeature *)feature;
            if (qr.messageString.length > 0) return qr.messageString;
        }
    }
    return nil;
}

@end

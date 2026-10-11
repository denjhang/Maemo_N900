#import <UIKit/UIKit.h>

@interface ResultViewController : UIViewController
- (id)initWithText:(NSString *)text format:(NSString *)format;
@end

@implementation ResultViewController {
    NSString   *_text;
    NSString   *_format;
    UITextView *_textView;
    UIButton   *_copyBtn;
    UIButton   *_openBtn;
}

- (id)initWithText:(NSString *)text format:(NSString *)format {
    self = [super init];
    if (self) {
        _text   = [text retain];
        _format = [format retain];
    }
    return self;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.title = _format;
    self.view.backgroundColor = [UIColor whiteColor];

    CGRect bounds = self.view.bounds;
    CGFloat pad   = 16.0f;

    UILabel *fmtLabel = [[UILabel alloc] initWithFrame:
        CGRectMake(pad, 60, bounds.size.width - pad * 2, 24)];
    fmtLabel.text      = _format;
    fmtLabel.font      = [UIFont systemFontOfSize:12.0f];
    fmtLabel.textColor = [UIColor grayColor];
    [self.view addSubview:fmtLabel];
    [fmtLabel release];

    _textView = [[UITextView alloc] initWithFrame:
        CGRectMake(pad, 92, bounds.size.width - pad * 2, 200)];
    _textView.text      = _text;
    _textView.font      = [UIFont systemFontOfSize:15.0f];
    _textView.editable  = NO;
    _textView.layer.borderWidth = 1.0f;
    _textView.layer.borderColor = [[UIColor lightGrayColor] CGColor];
    _textView.layer.cornerRadius = 6.0f;
    [self.view addSubview:_textView];

    _copyBtn = [UIButton buttonWithType:UIButtonTypeRoundedRect];
    _copyBtn.frame = CGRectMake(pad, 310, 130, 44);
    [_copyBtn setTitle:@"Copy" forState:UIControlStateNormal];
    [_copyBtn addTarget:self action:@selector(copyText)
        forControlEvents:UIControlEventTouchUpInside];
    [self.view addSubview:_copyBtn];

    BOOL isUrl = [_text hasPrefix:@"http://"] || [_text hasPrefix:@"https://"];
    if (isUrl) {
        _openBtn = [UIButton buttonWithType:UIButtonTypeRoundedRect];
        _openBtn.frame = CGRectMake(bounds.size.width - pad - 130, 310, 130, 44);
        [_openBtn setTitle:@"Open URL" forState:UIControlStateNormal];
        [_openBtn addTarget:self action:@selector(openURL)
            forControlEvents:UIControlEventTouchUpInside];
        [self.view addSubview:_openBtn];
    }
}

- (void)copyText {
    UIPasteboard *pb = [UIPasteboard generalPasteboard];
    pb.string = _text;
    UIAlertView *alert = [[UIAlertView alloc]
        initWithTitle:@"Copied"
        message:nil
        delegate:nil
        cancelButtonTitle:@"OK"
        otherButtonTitles:nil];
    [alert show];
    [alert release];
}

- (void)openURL {
    NSURL *url = [NSURL URLWithString:_text];
    if (url) [[UIApplication sharedApplication] openURL:url];
}

- (void)dealloc {
    [_text release];
    [_format release];
    [_textView release];
    [super dealloc];
}

@end

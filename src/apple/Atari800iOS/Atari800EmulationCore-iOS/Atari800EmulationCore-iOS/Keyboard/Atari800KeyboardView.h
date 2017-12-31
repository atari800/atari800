//
//  Atari800KeyboardView.h
//  Atari800Keyboard
//
//  Created by Rod Münch on 02/02/2012.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

@import UIKit;
@import QuartzCore;

#import <Atari800EmulationCore_iOS/Atari800Emulator.h>

@interface Atari800KeyboardView : UIView {

}

@property (nonatomic, weak) IBOutlet UIButton *startButton;
@property (nonatomic, weak) IBOutlet UIButton *selectButton;
@property (nonatomic, weak) IBOutlet UIButton *optionButton;

- (Atari800KeyboardHandler)newKeyboardHandler;

@end

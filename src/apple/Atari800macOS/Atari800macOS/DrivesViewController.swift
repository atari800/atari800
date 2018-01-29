//
//  DrivesViewController.swift
//  Atari800macOS
//
//  Created by Rod Münch on 07/01/2018.
//  Copyright © 2018 Atari800 development team. All rights reserved.
//

import Cocoa
import Atari800EmulationCore

class DrivesViewController: NSViewController {
    
    @IBOutlet var drivesController: Atari800DriveController?
    @IBOutlet weak var tableView: NSTableView?
    
    @IBAction func close(_ sender: Any?) {
        
        self.dismiss(sender);
    }
    
    @IBAction func mount(_ sender: Any?) {
        
        if let view = sender as? NSView {
        
            if let driveNumber = self.tableView?.row(for: view) {
                
                self.mount(driveNumber: driveNumber)
            }
        }
    }
    
    func mount(driveNumber: Int) {
        
        let panel = NSOpenPanel();
        panel.begin { (response: NSApplication.ModalResponse) in
            
            if response == NSApplication.ModalResponse.OK {
                
                if let url = panel.urls.first {
                    
                    if let emulator = Atari800Emulator.shared() {
                        
                        DispatchQueue.main.async {
                            
                            emulator.mount(url, driveNumber: driveNumber) { (ok, error) in
                                
                                NSDocumentController.shared.noteNewRecentDocumentURL(url)
                            
                                self.tableView?.reloadData()
                            }
                        }
                    }
                }
            }
        }
    }
}

extension DrivesViewController: NSTableViewDataSource {
    
    func numberOfRows(in tableView: NSTableView) -> Int {
        
        return 8
    }
}

extension DrivesViewController: NSTableViewDelegate {
    
    func cellIdentifier(for tableColumn: NSTableColumn?) -> NSUserInterfaceItemIdentifier? {
        
        if let tableColumn = tableColumn {
            
            switch (tableColumn.identifier) {
            case NSUserInterfaceItemIdentifier("Drive Number"):
                return NSUserInterfaceItemIdentifier("Drive Number Cell")
            case NSUserInterfaceItemIdentifier("File Name"):
                return NSUserInterfaceItemIdentifier("File Name Cell")
            case NSUserInterfaceItemIdentifier("Action"):
                return NSUserInterfaceItemIdentifier("Action Cell")
            case NSUserInterfaceItemIdentifier("Write Protect"):
                return NSUserInterfaceItemIdentifier("Write Protect Cell")
            default:
                break
            }
        }
        
        return nil
    }
    
    func setValue(for tableColumn: NSTableColumn?, view: NSTableCellView, drive: [AnyHashable: Any]) {
        
        if let tableColumn = tableColumn {
            
            switch (tableColumn.identifier) {
                
            case NSUserInterfaceItemIdentifier("Drive Number"):
                view.textField?.stringValue = String(describing: drive[Atari800DriveNumberKey]!)
                break
                
            case NSUserInterfaceItemIdentifier("File Name"):
                view.textField?.stringValue = drive[Atari800DriveFileNameKey] as! String
                break
                
            case NSUserInterfaceItemIdentifier("Action"):
                
                break
                
            case NSUserInterfaceItemIdentifier("Write Protect"):
                break
                
            default:
                break
            }
        }
    }
    
    func tableView(_ tableView: NSTableView, viewFor tableColumn: NSTableColumn?, row: Int) -> NSView? {
        
        guard let cellIdentifier = self.cellIdentifier(for: tableColumn) else {
            
            return nil
        }
        
        if let result = tableView.makeView(withIdentifier: cellIdentifier, owner: self) {
            
            if let drive = self.drivesController?.drives[row] {
                
                self.setValue(for: tableColumn!, view: result as! NSTableCellView, drive: drive)
            }
            
            return result
        }
        
        return nil
    }
}

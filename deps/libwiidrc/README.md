# libwiidrc
This is only usable with a special patched fw.img and homebrew injected into a WiiU VC title, details on the patches are below.  
Most WiiU VC injector tools already contain the patches required for this, the patches below are only needed if you do it manually.  
The usage is quite simple, call WiiDRC_Init on boot and after that call WiiDRC_ScanPads every time you update your pad inputs.  
To check if the gamepad is ready to use, call WiiDRC_Inited and WiiDRC_Connected, if both are true its good to go.  
The analog stick range goes from about -75 to 75 so make sure to scale it to whatever you need.  
The battery status goes from 6 (full) down to 1 (critical), 0 meaning that it is currently charging.  
See the "demo" folder for an example on how to use it in a bit more detail.  
Both compiled Library and Demo can be grabbed from the Releases tab.    

# Patches
When building a WiiU VC file to install, use my nfs2iso2nfs fork with the "-homebrew -passthrough" command line options to patch the fw.img with all the required libwiidrc patches.

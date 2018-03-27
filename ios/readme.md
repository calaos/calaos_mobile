Building for iOS
----------------

Building for iOS requires some modification of the project using XCode. QtCreator and QMake does not support all advanced iOS function like Push Notification target for building the service.

Unfortunately this has to be done everytime you clean the build folder, or rerun qmake (the xcodeproj is recreated).

Notes: I tried to use https://github.com/CocoaPods/Xcodeproj ruby lib to manipulate the xcode project, but the library does not support the project format created by qmake... too bad.

After running qmake, open the project into XCode.

### Capabilities Tab

Enable in this Tab:

 - Push Notifications
 - Keychain Sharing
  - With group: fr.calaos.CalaosMobile
 - App Groups
  - With group: group.fr.calaos.CalaosMobile
  
### Add a new target to the project for the Push Notification service

Add a new target: **Notification Service Extension**

 - Product Name: **PushNotif**
 - Choose the team
 - Language: Obj-C

#### In the new target, enable the Capabilities:

 - Keychain Sharing
  - With group: **fr.calaos.CalaosMobile**
 - App Groups
  - With group: **group.fr.calaos.CalaosMobile**

#### Setup files

Run this to remove the default template that xcode just created:

```
cd build-mobile-Qt_5_10_1_for_iOS-Debug
rm -fr PushNotif
ln -s ../calaos_mobile/ios/PushNotif .
```

#### Build settings fo the new target

Delete all arguments that are for Qt from the *Other Linker Flags* property.

#### Build Phases

##### Compile Sources

Add missing files for the PushNotif target, in the Sources tab:
 
 - A0SimpleKeychain.m
 - A0SimpleKeychain+KeyPair.m

# in_vgm Build Instructions

In order to build in_vgm you need to download the source of VGMPlay and extract it to `..\VGMPlay\`.
Then you can open `in_vgm.dsw` with MS Visual C++ to compile it.

in_vgm was made with MS VC++ 6. It should compile with later versions of MS Visual C++ too, but it will ask you to convert the project first.

## Debug Build Notes
To make testing easier, the `in_vgm.dll` gets copied to the `Winamp\Plugins` folder after the Debug build finished.
You may want to edit the post-build-command so that it fits with your Winamp PlugIn-directory.

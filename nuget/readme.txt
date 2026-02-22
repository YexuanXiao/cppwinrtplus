========================================================================
The YexuanXiao.CppWinRT Plus NuGet package automatically generates C++/WinRT projection headers, 
enabling you to both consume and produce Windows Runtime classes.
========================================================================

C++/WinRT Plus detects Windows metadata required by the project, from:
* Platform winmd files in the SDK (both MSI and NuGet)
* NuGet package references containing winmd files
* Other project references producing winmd files
* Raw winmd file references
* Interface definition language (IDL) files in the project 

For any winmd file discovered above, C++/WinRT Plus creates reference (consuming) projection headers.  
Client code can simply #include these headers, which are created in the generated files directory (see below).

For any IDL file contained in the project, C++/WinRT Plus creates component (producing) projection headers.  
In addition, C++/WinRT Plus generates templates and skeleton implementations for each runtime class, under the Generated Files directory.
  
========================================================================
For more information, visit:
https://github.com/YexuanXiao/cppwinrtplus/tree/master/nuget
========================================================================

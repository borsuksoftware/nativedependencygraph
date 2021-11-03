# nativedependencygraph
Native (c++) cross platform in-memory dependency graph.

Note that this is very similiar to our .net object graph library.

## Summary
This repository contains a simple, cross platform (only uses standard c++) in-memory dependency graph which can be used in a range of applications to build up a range of outputs. Its core features are:

* Multi-threaded safe - all functionality can be accessed from multiple threads, even concurrently.
* Ability to choose how objects are built - users can choose between single threaded mode, multi-threaded mode or can provide their own runners for more complicated scenarios

Coming soon:
* Ability to create child graphcs based off an existing graph
* Ability to have recursive dependencies (that is the set of dependencies required to build A are a function of the value of some of the dependencies of A, e.g. A depends on B and either C or D dependending upon the value of B)

## Architecture
The tool is based off multiple parts:

* ObjectContext - this is the dependency graph component, templatised with the address type and the expected values
* ObjectBuilder - a component which is responsible for providing the dependencies and then subsequently building the object once its dependencies are known
* ObjectBuilderProvider - a component which can provide an object builder for a given address
* JobQueue - the component which will perform the actual object building, can be single threaded, multi-threaded or completely customised

All requests to start the build process for an object should be made on the object context which can perform the necessary orchestrations, i.e. work out what is required to do in order to build the item, before pushing jobs to the job queue which has the responsibility of executing the jobs. Note that when a request has been made, control will be returned to the originally caller as soon as practically possible which means that it's up to the caller to wait (a wait handle is provided) on the object being ready. This applies to both building the object and sourcing the dependencies for building the object - the latter being necessary to allow support for recursive dependencies.

## FAQs
#### Why did you create this?
This was a native port of a piece of functionality that we had already written in .net. We wanted similar functionality in native code so we created it. Our initial intended use-case was in construction market objects from data for some financial analysis. Or maybe we did it just because we wanted to write some multi-threaded code. You can pick :-)

#### Can we use this in our project?
Please do. The only thing we ask (but not require) is that you let us know that you find the code useful. It really does provide encouragement to provide more functionality.

#### Is there a native nuget package provided for this code?
No, not currently. We don't provide one as there could be a wide range of supported builds / tool chains so we don't currently feel that it's worthwhile providing this. The other complication is that given that almost everything in the core functionality is templatised, then there's very little to go into a lib dll to subsequently link to (as opposed to simply including the header files as and when they're needed).

If you require this, then please get in touch or do feel free to create one and let us know. 

## Bugs etc.
If you come across a bug, then please let us know (or raise a PR to fix it)

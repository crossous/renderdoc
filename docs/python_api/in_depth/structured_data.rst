Structured Data
===============

RenderDoc includes a system for representing arbitrary structured data - including specific byte-sized types . This system is used internally for representing a readable form of the serialised data as well as for other systems like configuration (:func:`~renderdoc.GetConfigSetting`) and :doc:`annotations <../../window/annotation_viewer>` (:data:`~renderdoc.APIEvent.annotations`).

Structured Objects
------------------

Data is represented as a tree structure. Each object is represented by :class:`~renderdoc.SDObject` and can be either a leaf node (containing a single value) or a node with children (a structure or array). For example a structure will be represented by a :class:`~renderdoc.SDObject` with one child per structure member. An array will contain one child per array element.

The type of an object is defined by :data:`~renderdoc.SDObject.type` of type :class:`~renderdoc.SDType`. This gives the basic type, size in bytes, and name of the object type. it also contains a number of flags (:class:`~renderdoc.SDTypeFlags`) which can determine e.g. if this object was stored as a pointer and could be ``NULL``, if the object was an enumeration and so has both an integer and a string value. It also has hints for display such as if this object is considered 'important', or if this object is considered internal/hidden.

The value of an object is stored in an :class:`~renderdoc.SDObjectData`. In this value storage the size of the element is irrelevant, it is always stored with the maximum possible precision. In python without an unsigned/signed integer distinction care should be taken to access through the correct member especially when modifying the value of a structured object. For :data:`~renderdoc.SDBasic.Buffer` values they are not stored directly but instead as an index into a separate list of buffers. See below with serialised capture data for more information.

The :class:`~renderdoc.SDObject` object contains a number of helper accessors and functions for fetching its contents as different types as well as for modifying it if this is a mutable object. 

Serialised Capture data
-----------------------

.. warning::
    The structured data for a serialised capture is entirely undocumented and may change! You may find this data useful and generally it will closely match the expectation from API function calls, but that will not always be the case and you should not treat this data as guaranteed.

When a capture is loaded, the serialised data is all stored in a structured data representation, rooted at a :class:`~renderdoc.SDFile`. For normal opening of a capture, the contents of buffers are *not* stored as this would represent too much wasted memory that is rarely accessed. The structured data still contains everything except the contents of large buffer values.

To obtain the structured data for a capture including buffers it is necessary to use :doc:`capture_access` which will serialise and load a capture including buffer data. This can be done without replaying even when the capture is otherwise open in the UI.

The :class:`~renderdoc.SDFile` contains a number of :data:`~renderdoc.SDFile.chunks`, each of which corresponds to one self-contained serialised function call. Note that although most of the serialised function calls will be directly taken from the calls the application made, as in the warning above some of the serialised function calls will be internal to RenderDoc. In both cases the serialised form backwards compatibility is handled internally by RenderDoc's serialisation and may still change.

You can look up the chunk for a given API event using :data:`~renderdoc.APIEvent.chunkIndex` - as long as this is not set to :data:`~renderdoc.APIEvent.NoChunk` then it gives the index in the corresponding :data:`~renderdoc.SDFile.chunks` list.

Each chunk can be thought of as a nameless struct with children - for an API event, the children will usually correspond to input parameters, but again note that this rule is not guaranteed and some children may be return values or internal data. You should make use of the flags (:class:`~renderdoc.SDTypeFlags`) on object types to determine whether or not an object should be displayed or is considered hidden. For the purposes of displaying summary views of events you can also use the 'important' flags to indicate which parameters are most likely to be relevant to users and which should be given less priority for display.
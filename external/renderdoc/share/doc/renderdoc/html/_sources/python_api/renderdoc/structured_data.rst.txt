API Reference: Structured Data
==============================

This is the API reference for the functions, classes, and enums in the ``renderdoc`` module which represents the underlying interface that the UI is built on top of. For more high-level information and instructions on using the python API, see :doc:`../index`.

.. contents:: Sections
   :local:

.. currentmodule:: renderdoc

Type information
----------------

.. autoclass:: SDType
  :members:

.. autoclass:: SDBasic
  :members:

.. autoclass:: SDTypeFlags
  :members:

Objects
-------

.. autoclass:: SDObject
  :members:

.. autoclass:: SDObjectData
  :members:

.. autoclass:: SDObjectPODData
  :members:

Chunks
------

.. autoclass:: SDChunk
  :members:

.. autoclass:: SDChunkMetaData
  :members:

.. autoclass:: SDChunkFlags
  :members:

Structured File
---------------

.. autoclass:: SDFile
  :members:

Creation Helper Functions
-------------------------

.. autofunction:: renderdoc.makeSDArray
.. autofunction:: renderdoc.makeSDBool
.. autofunction:: renderdoc.makeSDEnum
.. autofunction:: renderdoc.makeSDFloat
.. autofunction:: renderdoc.makeSDInt32
.. autofunction:: renderdoc.makeSDInt64
.. autofunction:: renderdoc.makeSDResourceId
.. autofunction:: renderdoc.makeSDString
.. autofunction:: renderdoc.makeSDStruct
.. autofunction:: renderdoc.makeSDUInt32
.. autofunction:: renderdoc.makeSDUInt64

API Reference: Utilities
========================

This is the API reference for the functions, classes, and enums in the ``renderdoc`` module which represents the underlying interface that the UI is built on top of. For more high-level information and instructions on using the python API, see :doc:`../index`.

.. contents:: Sections
   :local:

.. currentmodule:: renderdoc

Maths
-----

.. autoclass:: FloatVector
  :members:

.. autofunction:: renderdoc.HalfToFloat
.. autofunction:: renderdoc.FloatToHalf

Logging & Versioning
--------------------

.. autofunction:: renderdoc.LogMessage
.. autofunction:: renderdoc.SetDebugLogFile
.. autofunction:: renderdoc.GetLogFile
.. autofunction:: renderdoc.GetCurrentProcessMemoryUsage
.. autofunction:: renderdoc.DumpObject

.. autoclass:: LogType
  :members:


Versioning
----------

.. autofunction:: renderdoc.GetVersionString
.. autofunction:: renderdoc.GetCommitHash
.. autofunction:: renderdoc.IsReleaseBuild

Settings
--------

.. autofunction:: renderdoc.GetConfigSetting
.. autofunction:: renderdoc.SetConfigSetting
.. autofunction:: renderdoc.SaveConfigSettings

Self-hosted captures
--------------------

.. autofunction:: renderdoc.CanSelfHostedCapture
.. autofunction:: renderdoc.StartSelfHostCapture
.. autofunction:: renderdoc.EndSelfHostCapture

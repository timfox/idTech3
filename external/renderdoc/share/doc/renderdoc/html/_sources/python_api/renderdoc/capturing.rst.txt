API Reference: Capturing
========================

This is the API reference for the functions, classes, and enums in the ``renderdoc`` module which represents the underlying interface that the UI is built on top of. For more high-level information and instructions on using the python API, see :doc:`../index`.

.. contents:: Sections
   :local:

.. currentmodule:: renderdoc

Execution & Injection
---------------------

.. autofunction:: renderdoc.ExecuteAndInject
.. autofunction:: renderdoc.InjectIntoProcess

.. autoclass:: renderdoc.CaptureOptions
  :members:
  
.. autofunction:: renderdoc.GetDefaultCaptureOptions

.. autoclass:: renderdoc.EnvironmentModification
  :members:

.. autoclass:: renderdoc.EnvMod
  :members:

.. autoclass:: renderdoc.EnvSep
  :members:

.. autoclass:: renderdoc.ExecuteResult
  :members:

Global Hooking
--------------

.. autofunction:: renderdoc.StartGlobalHook
.. autofunction:: renderdoc.StopGlobalHook
.. autofunction:: renderdoc.IsGlobalHookActive
.. autofunction:: renderdoc.CanGlobalHook

Target Control
--------------

.. autofunction:: renderdoc.EnumerateRemoteTargets
.. autofunction:: renderdoc.CreateTargetControl

.. autoclass:: renderdoc.TargetControl
  :members:

.. autoclass:: renderdoc.TargetControlMessage
  :members:

.. autoclass:: renderdoc.TargetControlMessageType
  :members:

.. autoclass:: renderdoc.NewCaptureData
  :members:

.. autoclass:: renderdoc.APIUseData
  :members:

.. autoclass:: renderdoc.BusyData
  :members:

.. autoclass:: renderdoc.NewChildData
  :members:


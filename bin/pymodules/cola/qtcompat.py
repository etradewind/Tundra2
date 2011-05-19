from PythonQt import QtGui
from PythonQt import QtCore

def install():
    if not hasattr(QtGui.QHBoxLayout, 'setContentsMargins'):
        QtGui.QHBoxLayout.setContentsMargins = lambda *args: True

    if not hasattr(QtGui.QVBoxLayout, 'setContentsMargins'):
        QtGui.QVBoxLayout.setContentsMargins = lambda *args: True


def add_search_path(prefix, path):
    if hasattr(QtCore.QDir, 'addSearchPath'):
        QtCore.QDir.addSearchPath(prefix, path)

def set_common_dock_options(window):
    if not hasattr(window, 'setDockOptions'):
        return
    nested = QtGui.QMainWindow.AllowNestedDocks
    tabbed = QtGui.QMainWindow.AllowTabbedDocks
    animated = QtGui.QMainWindow.AnimatedDocks
    window.setDockOptions(nested | tabbed | animated)

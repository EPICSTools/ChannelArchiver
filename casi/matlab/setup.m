
% Useful when files are on a SAMBA file system
% because otherwise Win32 Matlab does not learn
% about new files:
% (see helpwin change_notification)
system_dependent RemotePathPolicy TimecheckDirFile;
system_dependent RemoteCWDPolicy  TimecheckDirFile;

# remote_copy
Copy files from local-local,remote-local and remote-remote with recursive flag (for source file protocol )

Packages required: libcurl4-gnutls-dev

Compilation and Creating Binary:
g++ -o remotecp -lcurl remote_copy.cpp

You can copy this remotecp to /bin so that it can be used with adding its directory to $PATH variable
sudo cp remotecp /bin/remotecp
chmod 777 /bin/remotecp

Instructions::

Usage: remotecp [-r][-json][-src_usrpwd][-dst_usrpwd] -src src_file -dst dst_file;
    
-src     => Specify source file support protocols ftp(ftp://username:password@ip/path), smb(smb://ip/sharename), file(file:///path/to/file) 
    
-dst     => Specify dst file support protocols ftp, smb, file ;
-r       => This option is used to for recursive copy Note:: Only SRC_FILE type should be FILE type;
-json    => Output will be of type json;
-h       => for help;
-src_usrpwd 
-dst_usrpwd => These fields  are provided if src or dst is of protocol smb usage: -src_usrpwd username:password ;
     
     
Example: 1)file-file::> rcp -src file:///home/lordkrs/in/karthik.mp4 -dst file:///home/lordkrs/out/karthik.mp4;
         
         2)file-ftp::> rcp -src file:///home/lordkrs/in/karthik.mp4 -dst ftp://karthavya:karthavya@192.168.43.221/karthik.mp4;
         
         3)file-smb::> rcp -src file:///home/lordkrs/in/karthik.mp4 -dst smb://192.168.43.221/temp/karthik.mp4 -dst_usrpwd karthavya:karthavya;
              
         4)ftp-smb::> rcp -src ftp://karthavya:karthavya@192.168.43.221/karthik.mp4 -dst smb://192.168.43.221/temp/karthik.mp4 -dst_usrpwd karthavya:karthavya;
              
         5)recursive copy::>rcp -r -src file:///home/lordkrs/in -dst ftp://karthavya:karthavya@192.168.43.221/;


Please note that recursive copy only works if -src's protocol is file
Create sub directory doesn't work for protocol smb
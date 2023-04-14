flex -o "src\\thrift\\thriftl.cc" src/thrift/thriftl.ll 
bison -y -o "src\\thrift\\thrifty.cc" --defines="src\\thrift\\thrifty.hh" src/thrift/thrifty.yy

exit /B %ERRORLEVEL%

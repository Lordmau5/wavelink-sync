include(FetchContent)

FetchContent_Declare(ixwebsocket URL https://github.com/machinezone/IXWebSocket/archive/refs/tags/v11.4.6.tar.gz)

message("Fetching IXwebsocket from GitHub...")
FetchContent_MakeAvailable(ixwebsocket)
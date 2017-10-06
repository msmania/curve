#include <windows.h>
#include <iostream>
#include <fstream>
#include <curvecore.h>

using namespace curve;

void Log(LPCWSTR format, ...) {
  va_list v;
  va_start(v, format);
  vwprintf(format, v);
  va_end(v);
}

void show_usage() {
  std::wcout
    << L"Usage: curve [command] [args...]" << std::endl
    << std::endl
    << L"  -s <endpoint>  Run as an RPC server" << std::endl
    << std::endl
    << L"Command as an RPC client:" << std::endl
    << L"  -q <endpoint>                                  -- Quit RPC server" << std::endl
    << L"  -g <endpoint> <url> <viewWidth> <viewHeight>   -- Navigate to URL" << std::endl
    << L"  -ga <endpoint> <url> <viewWidth> <viewHeight>  -- Navigate async to URL" << std::endl
    << L"  -m <endpoint> [backFile]                       -- Set mapping file" << std::endl
    << L"  -c <endpoint> <bitCount> <captureLocal>        -- Capture the window" << std::endl
    << L"  -d <endpoint1> <endpoint2>" << std::endl
    << L"     <url> <wait> <viewWidth> <viewHeight>" << std::endl
    << L"     <backFile1> <backFile1> <algo> [diffImage]  -- Image diff'ing" << std::endl
    << L"     (algo: 0=skip | 1=average | 2=max | 3=min | 4=OpenCV)" << std::endl
    << L"  -batch <endpoint> <bitCount>" << std::endl
    << L"         <backFile1> <backFile1>                 -- Batch run" << std::endl
    << std::endl;
}

int wmain(int argc, wchar_t *argv[]) {
  if (argc >= 3 && wcscmp(argv[1], L"-s") == 0) {
    RunAsServer(argv[2]);
  }
  else if (argc >= 3 && wcscmp(argv[1], L"-q") == 0) {
    Shutdown(argv[2]);
  }
  else if (argc >= 6 && wcscmp(argv[1], L"-g") == 0) {
    Navigate(argv[2],
             argv[3],
             _wtoi(argv[4]),
             _wtoi(argv[5]),
             /*async*/false);
  }
  else if (argc >= 6 && wcscmp(argv[1], L"-ga") == 0) {
    Navigate(argv[2],
             argv[3],
             _wtoi(argv[4]),
             _wtoi(argv[5]),
             /*async*/true);
  }
  else if (argc >= 3 && wcscmp(argv[1], L"-m") == 0) {
    EnsureFileMapping(argv[2],
                      argc >= 4 ? argv[3] : nullptr,
                      /*forceUpdate*/true,
                      /*section*/nullptr);
  }
  else if (argc >= 5 && wcscmp(argv[1], L"-c") == 0) {
    Capture(argv[2], _wtoi(argv[3]), argv[4]);
  }
  else if (argc >= 11 && wcscmp(argv[1], L"-d") == 0) {
    DiffInput in;
    in.endpoint1 = argv[2];
    in.endpoint2 = argv[3];
    in.url = argv[4];
    in.waitInMilliseconds = _wtoi(argv[5]);
    in.viewWidth = _wtoi(argv[6]);
    in.viewHeight = _wtoi(argv[7]);
    in.backFile1 = argv[8];
    in.backFile2 = argv[9];
    in.algo = static_cast<DiffAlgorithm>(_wtoi(argv[10]));
    in.diffImage = argc >= 12 ? argv[11] : nullptr;
    DiffOutput out;
    if (SUCCEEDED(DiffImage(in, out))) {
      Log(L"Diff score: %f %f %f\n",
          out.psnr_area_vs_smooth,
          out.psnr_target_vs_area,
          out.psnr_target_vs_smooth);
    }
  }
  else if (argc >= 6 && wcscmp(argv[1], L"-batch") == 0) {
    BatchRun(std::cin, argv[2], argv[3], argv[4], argv[5]);
    //std::ifstream is("BATCH");
    //if (is.is_open()) {
    //  BatchRun(is, argv[2], argv[3], argv[4], argv[5]);
    //}
  }
  else {
    show_usage();
  }
  return 0;
}
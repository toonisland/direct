// Filename: panda3d.cxx
// Created by:  drose (03Jun09)
//
////////////////////////////////////////////////////////////////////
//
// PANDA 3D SOFTWARE
// Copyright (c) Carnegie Mellon University.  All rights reserved.
//
// All use of this software is subject to the terms of the revised BSD
// license.  You should have received a copy of this license along
// with this source code in a file named "LICENSE."
//
////////////////////////////////////////////////////////////////////

// This program must link with Panda for HTTPClient support.  This
// means it probably should be built with LINK_ALL_STATIC defined, so
// we won't have to deal with confusing .dll or .so files that might
// compete on the disk with the dynamically-loaded versions.  There's
// no competition in memory address space, though, because
// p3d_plugin--the only file we dynamically link in--doesn't itself
// link with Panda.

#include "pandabase.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include "httpClient.h"
#include "httpChannel.h"
#include "Ramfile.h"
#include "thread.h"
#include "p3d_plugin.h"
#include "pset.h"

#ifndef HAVE_GETOPT
  #include "gnu_getopt.h"
#else
  #ifdef HAVE_GETOPT_H
    #include <getopt.h>
  #endif
#endif

static const string default_plugin_filename = "libp3d_plugin";

P3D_initialize_func *P3D_initialize;
P3D_free_string_func *P3D_free_string;
P3D_create_instance_func *P3D_create_instance;
P3D_instance_finish_func *P3D_instance_finish;
P3D_instance_has_property_func *P3D_instance_has_property;
P3D_instance_get_property_func *P3D_instance_get_property;
P3D_instance_set_property_func *P3D_instance_set_property;
P3D_instance_get_request_func *P3D_instance_get_request;
P3D_check_request_func *P3D_check_request;
P3D_request_finish_func *P3D_request_finish;
P3D_instance_feed_url_stream_func *P3D_instance_feed_url_stream;

typedef pset<P3D_instance *> Instances;
Instances _instances;

class URLGetterThread : public Thread {
public:
  URLGetterThread(P3D_instance *instance,
                  int unique_id,
                  const URLSpec &url,
                  const string &post_data);
protected:
  virtual void thread_main();

private:
  P3D_instance *_instance;
  int _unique_id;
  URLSpec _url;
  string _post_data;
};

URLGetterThread::
URLGetterThread(P3D_instance *instance,
                int unique_id,
                const URLSpec &url,
                const string &post_data) :
  Thread(url, "URLGetter"),
  _instance(instance),
  _unique_id(unique_id),
  _url(url),
  _post_data(post_data)
{
}

void URLGetterThread::
thread_main() {
  HTTPClient *http = HTTPClient::get_global_ptr();

  cerr << "Getting URL " << _url << "\n";

  PT(HTTPChannel) channel = http->make_channel(false);
  if (_post_data.empty()) {
    channel->begin_get_document(_url);
  } else {
    channel->begin_post_form(_url, _post_data);
  }

  Ramfile rf;
  channel->download_to_ram(&rf);

  size_t bytes_sent = 0;
  while (channel->run()) {
    if (rf.get_data_size() != 0) {
      // Got some new data.
      P3D_instance_feed_url_stream
        (_instance, _unique_id, P3D_RC_in_progress,
         channel->get_status_code(),
         channel->get_file_size(),
         (const unsigned char *)rf.get_data().data(), rf.get_data_size());
      bytes_sent += rf.get_data_size();
      rf.clear();
    }
  }

  // All done.
  P3D_result_code status = P3D_RC_done;
  if (!channel->is_valid()) {
    if (channel->get_status_code() != 0) {
      status = P3D_RC_http_error;
    } else {
      status = P3D_RC_generic_error;
    }
  }

  P3D_instance_feed_url_stream
    (_instance, _unique_id, status,
     channel->get_status_code(),
     bytes_sent, NULL, 0);

  cerr << "Done getting URL " << _url << ", got " << bytes_sent << " bytes\n";
}

bool
load_plugin(const string &p3d_plugin_filename) {
  string filename = p3d_plugin_filename;
  if (filename.empty()) {
    // Look for the plugin along the path.
    filename = default_plugin_filename;
#ifdef _WIN32
    filename += ".dll";
#else
    filename += ".so";
#endif
  }

#ifdef _WIN32
  HMODULE module = LoadLibrary(filename.c_str());
  if (module == NULL) {
    // Couldn't load the DLL.
    return false;
  }

  // Get the full path to the DLL in case it was found along the path.
  static const buffer_size = 4096;
  static char buffer[buffer_size];
  if (GetModuleFileName(module, buffer, buffer_size) != 0) {
    if (GetLastError() != 0) {
      filename = buffer;
    }
  }
  cerr << filename << "\n";

  // Now get all of the function pointers.
  P3D_initialize = (P3D_initialize_func *)GetProcAddress(module, "P3D_initialize");  
  P3D_free_string = (P3D_free_string_func *)GetProcAddress(module, "P3D_free_string");  
  P3D_create_instance = (P3D_create_instance_func *)GetProcAddress(module, "P3D_create_instance");  
  P3D_instance_finish = (P3D_instance_finish_func *)GetProcAddress(module, "P3D_instance_finish");  
  P3D_instance_has_property = (P3D_instance_has_property_func *)GetProcAddress(module, "P3D_instance_has_property");  
  P3D_instance_get_property = (P3D_instance_get_property_func *)GetProcAddress(module, "P3D_instance_get_property");  
  P3D_instance_set_property = (P3D_instance_set_property_func *)GetProcAddress(module, "P3D_instance_set_property");  
  P3D_instance_get_request = (P3D_instance_get_request_func *)GetProcAddress(module, "P3D_instance_get_request");  
  P3D_check_request = (P3D_check_request_func *)GetProcAddress(module, "P3D_check_request");  
  P3D_request_finish = (P3D_request_finish_func *)GetProcAddress(module, "P3D_request_finish");  
  P3D_instance_feed_url_stream = (P3D_instance_feed_url_stream_func *)GetProcAddress(module, "P3D_instance_feed_url_stream");  
#endif  // _WIN32

  // Ensure that all of the function pointers have been found.
  if (P3D_initialize == NULL ||
      P3D_free_string == NULL ||
      P3D_create_instance == NULL ||
      P3D_instance_finish == NULL ||
      P3D_instance_has_property == NULL ||
      P3D_instance_get_property == NULL ||
      P3D_instance_set_property == NULL ||
      P3D_instance_get_request == NULL ||
      P3D_check_request == NULL ||
      P3D_request_finish == NULL ||
      P3D_instance_feed_url_stream == NULL) {
    return false;
  }

  // Successfully loaded.
  if (!P3D_initialize()) {
    // Oops, failure to initialize.
    return false;
  }

  return true;
}

void
handle_request(P3D_request *request) {
  bool handled = false;

  switch (request->_request_type) {
  case P3D_RT_stop:
    cerr << "Got P3D_RT_stop\n";
    P3D_instance_finish(request->_instance);
    _instances.erase(request->_instance);
#ifdef _WIN32
    // Post a silly message to spin the event loop.
    PostMessage(NULL, WM_USER, 0, 0);
#endif
    handled = true;
    break;

  case P3D_RT_get_url:
    cerr << "Got P3D_RT_get_url\n";
    {
      PT(URLGetterThread) thread = new URLGetterThread
        (request->_instance, request->_request._get_url._unique_id,
         URLSpec(request->_request._get_url._url), "");
      thread->start(TP_normal, false);
    }
    break;

  case P3D_RT_post_url:
    cerr << "Got P3D_RT_post_url\n";
    {
      PT(URLGetterThread) thread = new URLGetterThread
        (request->_instance, request->_request._post_url._unique_id,
         URLSpec(request->_request._post_url._url), 
         string(request->_request._post_url._post_data, request->_request._post_url._post_data_size));
      thread->start(TP_normal, false);
    }
    break;

  default:
    // Some request types are not handled.
    cerr << "Unhandled request: " << request->_request_type << "\n";
    break;
  };

  P3D_request_finish(request, handled);
}

#ifdef _WIN32
LONG WINAPI
window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  };

  return DefWindowProc(hwnd, msg, wparam, lparam);
}

void
make_parent_window(P3D_window_handle &parent_window, 
                   int win_width, int win_height) {
  WNDCLASS wc;

  HINSTANCE application = GetModuleHandle(NULL);
  ZeroMemory(&wc, sizeof(WNDCLASS));
  wc.lpfnWndProc = window_proc;
  wc.hInstance = application;
  wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
  wc.lpszClassName = "panda3d";

  if (!RegisterClass(&wc)) {
    cerr << "Could not register window class!\n";
    exit(1);
  }

  DWORD window_style = 
    WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX |
    WS_SIZEBOX | WS_MAXIMIZEBOX;

  HWND toplevel_window = 
    CreateWindow("panda3d", "Panda3D", window_style,
                 CW_USEDEFAULT, CW_USEDEFAULT, win_width, win_height,
                 NULL, NULL, application, 0);
  if (!toplevel_window) {
    cerr << "Could not create toplevel window!\n";
    exit(1);
  }

  ShowWindow(toplevel_window, SW_SHOWNORMAL);

  parent_window._hwnd = toplevel_window;
}
#endif // _WIN32

#ifdef __APPLE__

void
make_parent_window(P3D_window_handle &parent_window, 
                   int win_width, int win_height) {
  // TODO.
  assert(false);
}

#endif  // __APPLE__

void
usage() {
  cerr
    << "\nUsage:\n"
    << "   panda3d [opts] file.p3d [file_b.p3d file_c.p3d ...]\n\n"
  
    << "This program is used to execute a Panda3D application bundle stored\n"
    << "in a .p3d file.  Normally you only run one p3d bundle at a time,\n"
    << "but it is possible to run multiple bundles simultaneously.\n\n"

    << "Options:\n\n"

    << "  -p p3d_plugin.dll\n"
    << "    Specify the full path to the particular Panda plugin DLL to\n"
    << "    run.  Normally, this will be found by searching in the usual\n"
    << "    places.\n\n"

    << "  -t [toplevel|embedded|fullscreen|hidden]\n"
    << "    Specify the type of graphic window to create.  If you specify "
    << "    \"embedded\", a new window is created to be the parent.\n\n"

    << "  -s width,height\n"
    << "    Specify the size of the graphic window.\n\n"

    << "  -o x,y\n"
    << "    Specify the position (origin) of the graphic window on the\n"
    << "    screen, or on the parent window.\n\n";
}

bool
parse_int_pair(char *arg, int &x, int &y) {
  char *endptr;
  x = strtol(arg, &endptr, 10);
  if (*endptr == ',') {
    y = strtol(endptr + 1, &endptr, 10);
    if (*endptr == '\0') {
      return true;
    }
  }

  // Some parse error on the string.
  return false;
}

int
main(int argc, char *argv[]) {
  extern char *optarg;
  extern int optind;
  const char *optstr = "p:t:s:o:h";

  string p3d_plugin_filename;
  P3D_window_type window_type = P3D_WT_toplevel;
  int win_x = 0, win_y = 0;
  int win_width = 0, win_height = 0;

  int flag = getopt(argc, argv, optstr);

  while (flag != EOF) {
    switch (flag) {
    case 'p':
      p3d_plugin_filename = optarg;
      break;

    case 't':
      if (strcmp(optarg, "toplevel") == 0) {
        window_type = P3D_WT_toplevel;
      } else if (strcmp(optarg, "embedded") == 0) {
        window_type = P3D_WT_embedded;
      } else if (strcmp(optarg, "fullscreen") == 0) {
        window_type = P3D_WT_fullscreen;
      } else if (strcmp(optarg, "hidden") == 0) {
        window_type = P3D_WT_hidden;
      } else {
        cerr << "Invalid value for -t: " << optarg << "\n";
        return 1;
      }
      break;

    case 's':
      if (!parse_int_pair(optarg, win_width, win_height)) {
        cerr << "Invalid value for -s: " << optarg << "\n";
        return 1;
      }
      break;

    case 'o':
      if (!parse_int_pair(optarg, win_x, win_y)) {
        cerr << "Invalid value for -o: " << optarg << "\n";
        return 1;
      }
      break;

    case 'h':
    case '?':
    default:
      usage();
      return 1;
    }
    flag = getopt(argc, argv, optstr);
  }

  argc -= (optind-1);
  argv += (optind-1);

  if (argc < 2) {
    usage();
    return 1;
  }

  if (!load_plugin(p3d_plugin_filename)) {
    cerr << "Unable to load Panda3D plugin.\n";
    return 1;
  }

  int num_instances = argc - 1;

  P3D_window_handle parent_window;
  if (window_type == P3D_WT_embedded) {
    // The user asked for an embedded window.  Create a toplevel
    // window to be its parent, of the requested size.
    if (win_width == 0 && win_height == 0) {
      win_width = 640;
      win_height = 480;
    }

    make_parent_window(parent_window, win_width, win_height);
    
    // Center the child window(s) within the parent window.
#ifdef _WIN32
    RECT rect;
    GetClientRect(parent_window._hwnd, &rect);

    win_x = (int)(rect.right * 0.1);
    win_y = (int)(rect.bottom * 0.1);
    win_width = (int)(rect.right * 0.8);
    win_height = (int)(rect.bottom * 0.8);
#endif

    // Subdivide the window into num_x_spans * num_y_spans sub-windows.
    int num_y_spans = int(sqrt((double)num_instances));
    int num_x_spans = (num_instances + num_y_spans - 1) / num_y_spans;
    
    int inst_width = win_width / num_x_spans;
    int inst_height = win_height / num_y_spans;

    for (int yi = 0; yi < num_y_spans; ++yi) {
      for (int xi = 0; xi < num_x_spans; ++xi) {
        int i = yi * num_x_spans + xi;
        if (i >= num_instances) {
          continue;
        }

        // Create instance i at window slot (xi, yi).
        int inst_x = win_x + xi * inst_width;
        int inst_y = win_y + yi * inst_height;

        P3D_instance *inst = P3D_create_instance
          (NULL, argv[i + 1], 
           P3D_WT_embedded, inst_x, inst_y, inst_width, inst_height, parent_window,
           NULL, 0);
        _instances.insert(inst);
      }
    }

  } else {
    // Not an embedded window.  Create each window with the same parameters.
    for (int i = 0; i < num_instances; ++i) {
      P3D_instance *inst = P3D_create_instance
        (NULL, argv[i + 1], 
         window_type, win_x, win_y, win_width, win_height, parent_window,
         NULL, 0);
      _instances.insert(inst);
    }
  }

#ifdef _WIN32
  if (window_type == P3D_WT_embedded) {
    // Wait for new messages from Windows, and new requests from the
    // plugin.
    MSG msg;
    int retval;
    retval = GetMessage(&msg, NULL, 0, 0);
    while (retval != 0 && !_instances.empty()) {
      if (retval == -1) {
        cerr << "Error processing message queue.\n";
        exit(1);
      }
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      
      // Check for new requests from the Panda3D plugin.
      P3D_instance *inst = P3D_check_request(false);
      while (inst != (P3D_instance *)NULL) {
        P3D_request *request = P3D_instance_get_request(inst);
        if (request != (P3D_request *)NULL) {
          handle_request(request);
        }
        inst = P3D_check_request(false);
      }
      retval = GetMessage(&msg, NULL, 0, 0);
    }
    
    cerr << "WM_QUIT\n";
    // WM_QUIT has been received.  Terminate all instances, and fall
    // through.
    Instances::iterator ii;
    for (ii = _instances.begin(); ii != _instances.end(); ++ii) {
      P3D_instance_finish(*ii);
    }
    _instances.clear();

  } else {
    // Not an embedded window, so we don't have our own window to
    // generate Windows events.  Instead, just wait for requests.
    P3D_instance *inst = P3D_check_request(true);
    while (inst != (P3D_instance *)NULL) {
      P3D_request *request = P3D_instance_get_request(inst);
      if (request != (P3D_request *)NULL) {
        handle_request(request);
      }
      inst = P3D_check_request(true);
    }
  }
    
#endif

  // Now wait while we process pending requests.
  P3D_instance *inst = P3D_check_request(true);
  while (inst != (P3D_instance *)NULL) {
    P3D_request *request = P3D_instance_get_request(inst);
    if (request != (P3D_request *)NULL) {
      handle_request(request);
    }
    inst = P3D_check_request(true);
  }

  // All instances have finished; we can exit.

  return 0;
}

#pragma once
#include "Conn.h"
#include "ConfirmDlg.h"
#include "ConnNode.h"
#include "Dlg.h"
#include "FolderMenu.h"
#include "FormEditFolder.h"
#include "FormEditRepository.h"
#include "MsgDlg.h"
#include "PanelMenu.h"
#include "SchemaMenu.h"
#include <SDL3/SDL.h>
#include <vector>

struct App
{
  SDL_Window   *win = nullptr;
  SDL_Renderer *ren = nullptr;
  int           ww = 1280, wh = 720;

  std::vector<ConnNode> conns;
  Dlg                   dlg;
  FormEditRepository    repo_dlg;
  FormEditFolder        folder_dlg;
  MsgDlg                msg_dlg;
  ConfirmDlg            confirm_dlg;

  int         pending_delete_idx          = -1;  // connection
  int         pending_delete_folder_conn  = -1;  // folder
  int         pending_delete_folder_repo  = -1;
  std::string pending_delete_folder_id;

  int       h_item   = -1;
  int       h_edit   = -1;
  bool      h_add    = false;
  float     mx       = 0;
  float     my       = 0;
  bool      lmb_held = false;
  PanelMenu  panel_menu;
  RepoMenu   repo_menu;
  FolderMenu folder_menu;

  void reload_conns()
  {
    const std::vector<Conn> fresh = load_all();
    std::vector<ConnNode>   updated;
    updated.reserve(fresh.size());
    for (auto &c : fresh) {
      ConnNode node{c, false, {}};
      for (auto &old : conns) {
        if (old.conn.name == c.name) {
          node.open  = old.open;
          node.repos = old.repos;
          break;
        }
      }
      updated.push_back(std::move(node));
    }
    conns = std::move(updated);
  }
};

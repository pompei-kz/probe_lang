#pragma once
#include "ConfirmDlg.h"
#include "Dlg.h"
#include "EditorView.h"
#include "FolderMenu.h"
#include "FormEditFolder.h"
#include "FormEditRepository.h"
#include "FormEditUnit.h"
#include "FormSelectUnit.h"
#include "MsgDlg.h"
#include "PanelMenu.h"
#include "SchemaMenu.h"
#include "UnitMenu.h"
#include "back/model/ConnNode.h"
#include "back/service/ConnServiceR.h"
#include <SDL3/SDL.h>
#include <vector>

namespace front {

  struct App
  {
    SDL_Window   *win = nullptr;
    SDL_Renderer *ren = nullptr;
    int           ww = 1280, wh = 720;

    std::vector<back::model::ConnNode> conns;
    Dlg                                dlg;
    FormEditRepository                 repo_dlg;
    FormEditFolder                     folder_dlg;
    FormEditUnit                       unit_dlg;
    MsgDlg                             msg_dlg;
    ConfirmDlg                         confirm_dlg;
    EditorView                         editor;
    FormSelectUnit                     sel_unit_form; // pick a unit for a field's Unit expression

    int         pending_delete_idx         = -1; // connection
    int         pending_delete_folder_conn = -1; // folder
    int         pending_delete_folder_repo = -1;
    std::string pending_delete_folder_id;
    int         pending_delete_unit_conn = -1; // unit
    int         pending_delete_unit_repo = -1;
    std::string pending_delete_unit_id;

    std::string sel_key; // key of the currently-selected tree node (keyboard navigation)

    int        h_item   = -1;
    int        h_edit   = -1;
    bool       h_add    = false;
    float      mx       = 0;
    float      my       = 0;
    bool       lmb_held = false;
    PanelMenu  panel_menu;
    RepoMenu   repo_menu;
    FolderMenu folder_menu;
    UnitMenu   unit_menu;

    void reload_conns()
    {
      const std::vector<back::model::Conn> fresh = back::load_all();
      std::vector<back::model::ConnNode>   updated;
      updated.reserve(fresh.size());
      for (auto &c : fresh) {
        back::model::ConnNode node{c, false, {}};
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

} // namespace front

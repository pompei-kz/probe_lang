#pragma once
#include "App.h"
#include "Clr.h"
#include "Dlg.h"
#include "DlgMouse.h"
#include <SDL3/SDL.h>

namespace front {

  void draw_plus(SDL_Renderer *r, float cx, float cy, float sz, Clr c);
  void draw_pencil(SDL_Renderer *r, float cx, float cy, float sz, Clr c);

  // returns 0=open, 1=saved, -1=cancelled
  int  dlg_render(SDL_Renderer *ren, Dlg &d, const DlgMouse &m);
  void panel_render(SDL_Renderer *ren, App &app, bool click, bool rclick, bool dblclick);

  // Reopen the branches that were open last time (persisted by ProjectTreeService).
  // Call once after the connections are loaded, before the first render.
  void restore_tree_open_state(App &app);

  // Re-apply the persisted open state to one connection's repos and their folders.
  // Use after reloading a connection's repos (e.g. connect_and_load).
  void restore_open_repos_and_folders(back::model::ConnNode &node);

  // Re-apply the persisted open state to a single repo's folder tree.
  // Use after reloading a repo's folders (e.g. load_repo_folders).
  void restore_repo_folders_open(const std::string &conn_name, back::model::RepoNode &repo);

  // Open (and persist) the parent of a just-added folder so the new folder is
  // visible. parent_folder_id empty means the parent is the repo itself.
  void open_added_folder_parent(const std::string &conn_name, back::model::RepoNode &repo, const std::string &parent_folder_id);

  // Keyboard handling for the tree panel: routes to an open popup menu, otherwise
  // navigates the selected node (arrows) or opens its context menu (Menu key).
  void panel_key_down(App &app, SDL_Keycode key);

} // namespace front

// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <gtk/gtk.h>

#include "model/SurfaceState.h"

// Edits a detached value; cancelling has no model or history side effects.
bool editPageSurface(GtkWindow* parent, SurfaceState& state);

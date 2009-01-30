/* vi: set ts=2:
 *
 *	
 *	Eressea PB(E)M host Copyright (C) 1998-2003
 *      Christian Schlittchen (corwin@amber.kn-bremen.de)
 *      Katja Zedel (katze@felidae.kn-bremen.de)
 *      Henning Peters (faroul@beyond.kn-bremen.de)
 *      Enno Rehling (enno@eressea.de)
 *      Ingo Wilken (Ingo.Wilken@informatik.uni-oldenburg.de)
 *
 * This program may not be used, modified or distributed without
 * prior permission by the authors of Eressea.
 */

#ifndef TELEPORT_H
#define TELEPORT_H
#ifdef __cplusplus
extern "C" {
#endif

  struct region *r_standard_to_astral(const struct region *r);
  struct region *r_astral_to_standard(const struct region *);
  extern struct region_list *astralregions(const struct region * rastral, boolean (*valid)(const struct region *));
  extern struct region_list *all_in_range(const struct region *r, short n, boolean (*valid)(const struct region *));
  extern boolean inhabitable(const struct region * r);
  extern boolean is_astral(const struct region * r);
  extern struct plane * get_astralplane(void);
  extern struct plane * get_normalplane(void);

  void create_teleport_plane(void);
  void set_teleport_plane_regiontypes(void);
  void spawn_braineaters(float chance);

#ifdef __cplusplus
}
#endif
#endif

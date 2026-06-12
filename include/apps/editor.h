#ifndef GOS_EDITOR_H
#define GOS_EDITOR_H

void editor_init(int wid);
void editor_update(int wid);
void editor_on_key(int wid, char c);
void editor_draw(int wid);

#endif // GOS_EDITOR_H
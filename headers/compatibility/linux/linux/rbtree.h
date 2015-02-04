/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef _LINUX_RBTREE_H
#define _LINUX_RBTREE_H


#define RB_NONE		0
#define RB_RED		1
#define RB_BLACK 	2


struct rb_node {
	int colour;
	struct rb_node* parent;
	struct rb_node* rb_left;
	struct rb_node* rb_right;
};

#define rb_parent(r) 	((struct rb_node*)((r)->parent))
#define rb_color(r) 	((struct rb_node*)((r)->colour))

#define rb_is_red(r)	(rb_color(r) == RB_RED)
#define rb_is_black(r)	(rb_color(r) == RB_BLACK)

#define rb_set_parent(r, p)	(rb_parent(r) = (p))
#define rb_set_color(r, c)	(rb_color(r) = (c))

#define rb_entry(ptr, type, member)	container_of(ptr, type, member)


#endif

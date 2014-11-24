#ifndef XML_H
#define XML_H

#include "prvm_cmds.h"

void VM_xml_open(prvm_prog_t *prog);
void VM_xml_close(prvm_prog_t *prog);
void VM_xml_tree_name(prvm_prog_t *prog);
void VM_xml_tree_text(prvm_prog_t *prog);
void VM_xml_tree_leaf(prvm_prog_t *prog);
void VM_xml_tree_child(prvm_prog_t *prog);
void VM_xml_tree_parent(prvm_prog_t *prog);
void VM_xml_tree_has_sibling(prvm_prog_t *prog);
void VM_xml_tree_next(prvm_prog_t *prog);
void VM_xml_tree_type(prvm_prog_t *prog);
void VM_xml_tree_root(prvm_prog_t *prog);
void VM_xml_tree_attribute(prvm_prog_t *prog);

void XML_Close(prvm_prog_t *prog, int index);

#endif

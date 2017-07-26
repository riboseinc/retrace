#ifndef __IOV_HELPERS_H__
#define __IOV_HELPERS_H__

void postcall_read_buf_fixup(struct postcall_read *call);

void precall_strncpy_src_fixup(struct precall_strncpy *call);
void postcall_strncpy_fixup(struct postcall_strncpy *call);
void postcall_strncpy_dest_fixup(struct postcall_strncpy *call);
void postcall_strncpy_src_fixup(struct postcall_strncpy *call);

void postcall_readdir_r_entry_fixup(struct postcall_readdir_r *call);

void precall_write_buf_fixup(struct precall_write *call);
void postcall_write_buf_fixup(struct postcall_write *call);

void precall_memmove_src_fixup(struct precall_memmove *call);
void postcall_memmove_dest_fixup(struct postcall_memmove *call);
void postcall_memmove_src_fixup(struct postcall_memmove *call);

void precall_memcpy_src_fixup(struct precall_memcpy *call);
void postcall_memcpy_dest_fixup(struct postcall_memcpy *call);
void postcall_memcpy_src_fixup(struct postcall_memcpy *call);
#endif

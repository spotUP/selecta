// '17/10/11 pxtnData.

#ifndef pxtnData_H
#define pxtnData_H

#include "pxtn.h"

typedef bool (* pxtnIO_r   )( void* user,       void* p_dst, int32_t size, int32_t num );
typedef bool (* pxtnIO_w   )( void* user, const void* p_dst, int32_t size, int32_t num );
typedef bool (* pxtnIO_seek)( void* user,       int   mode , int32_t size              );
typedef bool (* pxtnIO_pos )( void* user,                    int32_t* p_pos            );

class pxtnData
{
private:

	void operator = (const pxtnData& src){}
	pxtnData        (const pxtnData& src){}

protected:

	bool        _b_init  ;

	pxtnIO_r    _io_read ;
	pxtnIO_w    _io_write;
	pxtnIO_seek _io_seek ;
	pxtnIO_pos  _io_pos  ;

	void _release();

	bool    _data_w_v     ( void* desc, int32_t   v, int32_t* p_add ) const;
	bool    _data_r_v     ( void* desc, int32_t* pv                 ) const;
	bool    _data_get_size( void* desc, int32_t* p_size             ) const;
	int32_t _data_check_v_size( uint32_t v ) const;

	// I427e7: read a little-endian file field or struct and convert it to host order (see pxtn_le_to_native).
	bool    _io_read_le   ( void* desc, void* p, const char* layout ) const;

	void _set_io_funcs( pxtnIO_r io_read, pxtnIO_w io_write, pxtnIO_seek io_seek, pxtnIO_pos io_pos );

public :

	pxtnData();
	virtual ~pxtnData();

	bool copy_from( const pxtnData* src );

	bool init();
	bool Xxx ();
};

// I427e7: the .ptcop/.ptnoise/.ptvoice formats are little-endian. These assemble values from bytes, so they are
// correct on every host (identity on little-endian) with no byte-order #ifdef. A layout names the fields of a
// packed file struct in order: 'b' one byte, 'w' 16 bits, 'l' 32 bits (int or float); a decimal count repeats the
// next token ("16bww" = char[16], u16, u16). Every file struct pxtone reads has natural alignment, so the layout
// size equals sizeof() on both x86 and m68k.
int32_t pxtn_le_layout_size( const char* layout );
void    pxtn_le_to_native  ( void* p, const char* layout );
void    pxtn_le16_to_native( void* p, int32_t count );

#endif

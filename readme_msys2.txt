pacman -S mingw-w64-x86_64-luajit mingw-w64-x86_64-protobuf mingw-w64-x86_64-sqlite3 mingw-w64-x86_64-boost mingw-w64-x86_64-rapidjson mingw-w64-x86_64-shapelib

/ming64/include/wingdi.h Polygon clashes with boost:: Polygon
->   //  WINGDIAPI WINBOOL WINAPI Polygon(HDC hdc,CONST POINT *apt,int cpt);

uint type undefined -> need to do global replace uint -> unsigned int
a boost header has "typedef unsigned int uint;" in it.. could also include that everywhere

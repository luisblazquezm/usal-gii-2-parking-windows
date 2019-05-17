#ifdef PARKING2_EXPORTS
#define PARKING2_API __declspec(dllexport)
#else
#define PARKING2_API __declspec(dllimport)
#endif

void PERROR(char const *a) 
 {LPVOID lpMsgBuf;
  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                GetLastError(),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR) &lpMsgBuf,0,NULL);
  fprintf(stderr,"%s:(%d)%s\n",a,GetLastError(),(char*)lpMsgBuf);
  LocalFree(lpMsgBuf);}

#define PRIMER_AJUSTE 0
#define SIGUIENTE_AJUSTE 1
#define MEJOR_AJUSTE 2
#define PEOR_AJUSTE 3 

typedef int HCoche;

typedef int  (*TIPO_FUNCION_LLEGADA)              (HCoche hc);
typedef int  (*TIPO_FUNCION_SALIDA)               (HCoche hc);
typedef void (*TIPO_FUNCION_APARCAR_COMMIT)       (HCoche hc);
typedef void (*TIPO_FUNCION_PERMISO_AVANCE)       (HCoche hc);
typedef void (*TIPO_FUNCION_PERMISO_AVANCE_COMMIT)(HCoche hc);

#ifdef PARKING2_EXPORTS
extern "C" PARKING2_API int PARKING2_inicio(TIPO_FUNCION_LLEGADA *,TIPO_FUNCION_SALIDA *,long,int);
extern "C" PARKING2_API int PARKING2_fin();
extern "C" PARKING2_API int PARKING2_aparcar(HCoche,void *datos,TIPO_FUNCION_APARCAR_COMMIT,
                                             TIPO_FUNCION_PERMISO_AVANCE,
                                             TIPO_FUNCION_PERMISO_AVANCE_COMMIT);
extern "C" PARKING2_API int PARKING2_desaparcar(HCoche,void *datos,
                                             TIPO_FUNCION_PERMISO_AVANCE,
                                             TIPO_FUNCION_PERMISO_AVANCE_COMMIT);
extern "C" PARKING2_API int PARKING2_getNUmero(HCoche);
extern "C" PARKING2_API int PARKING2_getLongitud(HCoche);
extern "C" PARKING2_API int PARKING2_getPosiciOnEnAcera(HCoche);
extern "C" PARKING2_API unsigned long PARKING2_getTServ(HCoche);
extern "C" PARKING2_API int PARKING2_getColor(HCoche);
extern "C" PARKING2_API void * PARKING2_getDatos(HCoche);
extern "C" PARKING2_API int PARKING2_getX(HCoche);
extern "C" PARKING2_API int PARKING2_getY(HCoche);
extern "C" PARKING2_API int PARKING2_getX2(HCoche);
extern "C" PARKING2_API int PARKING2_getY2(HCoche);
extern "C" PARKING2_API int PARKING2_getAlgoritmo(HCoche);
extern "C" PARKING2_API int PARKING2_isAceraOcupada(int algoritmo,int pos);
#endif

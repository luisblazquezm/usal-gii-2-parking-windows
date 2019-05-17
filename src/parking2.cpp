#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include "parking2.h"

/**************************/
/*   Headers manuales     */
/**************************/

#define ROAD_LENGTH				80
#define EMPTY					0
#define RESERVED				1

#define LOTS					0
#define STRIPE					1
#define LANE					2

#define N_AJUSTES				4

#define TEN_THOUSAND			10000

/* STRUCTS DE ARGUMENTOS DE HILO */
struct PARKING_inicio_args {
	TIPO_FUNCION_LLEGADA *aFuncionesLlegada;
	TIPO_FUNCION_SALIDA *aFuncionesSalida;
	long intervalo;
	int d;
};

/* Punteros a las funciones */
int(*PARKING_inicio)(TIPO_FUNCION_LLEGADA *, TIPO_FUNCION_SALIDA *, long, int);
int(*PARKING_fin)(void);
int(*PARKING_aparcar)(HCoche, void *datos, TIPO_FUNCION_APARCAR_COMMIT,
	TIPO_FUNCION_PERMISO_AVANCE,
	TIPO_FUNCION_PERMISO_AVANCE_COMMIT);
int(*PARKING_desaparcar)(HCoche, void *datos,
	TIPO_FUNCION_PERMISO_AVANCE,
	TIPO_FUNCION_PERMISO_AVANCE_COMMIT);
int(*PARKING_getNUmero)(HCoche);
int(*PARKING_getLongitud)(HCoche);
int(*PARKING_getPosiciOnEnAcera)(HCoche);
unsigned long(*PARKING_getTServ)(HCoche);
int(*PARKING_getColor)(HCoche);
void * (*PARKING_getDatos)(HCoche);
int(*PARKING_getX)(HCoche);
int(*PARKING_getY)(HCoche);
int(*PARKING_getX2)(HCoche);
int(*PARKING_getY2)(HCoche);
int(*PARKING_getAlgoritmo)(HCoche);
int(*PARKING_isAceraOcupada)(int algoritmo, int pos);

/* Funciones de llegada */
int LlegadaPrimerAjuste(HCoche hc);
int LlegadaSiguienteAjuste(HCoche hc);
int LlegadaMejorAjuste(HCoche hc);
int LlegadaPeorAjuste(HCoche hc);

int SalidaPrimerAjuste(HCoche hc);
int SalidaSiguienteAjuste(HCoche hc);
int SalidaMejorAjuste(HCoche hc);
int SalidaPeorAjuste(HCoche hc);

int SiguienteAjustePrimerHueco(int last_pos);
int SiguienteAjusteAjustaPrimer(int c_length, int a, int b);

/* Otras funciones */
int TestArgs(int argc, char * argv[]);
int InvalidOptionMsg(char * opt);
int HelpMsg(void);
int ShortHelpMsg(void);
int LoadParkingDll();
void AparcarCommit(HCoche hc);
void PermisoAvance(HCoche hc);
void PermisoAvanceCommit(HCoche hc);
int CreateIPC();
DWORD WINAPI HiloParkingInicio(LPVOID param);
DWORD WINAPI HiloAparcar(LPVOID param);
DWORD WINAPI HiloDesaparcar(LPVOID param);
int CloseIPC();
void printCarretera(); // __DEBUG__ ONLY


// Descomentar líneas para desactivar ajustes
//#define PRIMER_AJUSTE_OFF
//#define SIGUIENTE_AJUSTE_OFF
//#define MEJOR_AJUSTE_OFF
//#define PEOR_AJUSTE_OFF

#define ARRAY_SYNC // Comentar esta línea si se desea sincronizacion con bucle espera (no ocupada)
#if !defined(LOOPING_SYNC) && !defined(ARRAY_SYNC)
#define LOOPING_SYNC
#endif

struct IPC {
	int nLotsArray[N_AJUSTES][ROAD_LENGTH];					// Acera
	HANDLE hRoadSemaphores[N_AJUSTES][ROAD_LENGTH];			// Carretera 

#ifdef ARRAY_SYNC
	HANDLE aTenThousandSemaphoresArray[N_AJUSTES][TEN_THOUSAND];
#elif // LOOPING_SYNC
	HANDLE hMutexOrdenCoches[N_AJUSTES];
	HANDLE hEventOrdenCoches[N_AJUSTES];
	int nNextCar[N_AJUSTES];
#endif

	HINSTANCE controladoraDLL;
};

struct Car {
	HCoche hCocheSM;
};

struct IPC IpcResources;
struct Car SharedMemory;

int main(int argc, char *argv[])
{
	int nIntervalo, nDebugFlag;
	struct PARKING_inicio_args piargs;

	TIPO_FUNCION_LLEGADA aFuncionesLlegada[N_AJUSTES] = { 
		LlegadaPrimerAjuste,
		LlegadaSiguienteAjuste,
		LlegadaMejorAjuste,
		LlegadaPeorAjuste
	};
	
	TIPO_FUNCION_SALIDA aFuncionesSalida[N_AJUSTES] = {
		SalidaPrimerAjuste,
		SalidaSiguienteAjuste,
		SalidaMejorAjuste,
		SalidaPeorAjuste
	};

	if (-1 == LoadParkingDll()) {
		fprintf(stderr, "%s\n", "Main: LoadParkingDll");
	}

	/* Creación de Carretera de MUTEXES*/
	if (CreateIPC() == -1) {
		fprintf(stderr, "%s\n", "Main: CreateIPC");
		return -1;
	}

	nDebugFlag = TestArgs(argc, argv);
	if (-1 == nDebugFlag)
		return -1;
	else
		nIntervalo = atoi(argv[1]); // Primer argumento, velocidad

	piargs.aFuncionesLlegada = aFuncionesLlegada;
	piargs.aFuncionesSalida = aFuncionesSalida;
	piargs.intervalo = nIntervalo;
	piargs.d = nDebugFlag;


	if (NULL == CreateThread(NULL, 0, HiloParkingInicio, (void *)&piargs, 0, NULL)) {
		PERROR("main: CreateThread: HiloParkingInicio");
		return -1;
	}

	Sleep(30000);

	if (-1 == PARKING_fin()) {
		fprintf(stderr, "%s\n", "PARKING_fin: finalizado con error");
		return -1;
	}

	if (CloseIPC() == -1) {
		fprintf(stderr, "%s\n", "Main: CloseIPC");
		return -1;
	}

	FreeLibrary(IpcResources.controladoraDLL);

    return 0;
}

int LoadParkingDll()
{
	HINSTANCE controladorDll;

	controladorDll = LoadLibrary("parking2.dll");
	if (NULL == controladorDll) {
		fprintf(stderr, "%s\n", "ErrorLoadLibrary");
		return -1;
	}

	IpcResources.controladoraDLL = controladorDll;

	PARKING_inicio = (int(*)(TIPO_FUNCION_LLEGADA *, TIPO_FUNCION_SALIDA *, LONG, BOOL)) GetProcAddress(controladorDll, "PARKING2_inicio");
	if (PARKING_inicio == NULL) {
		PERROR("LoadParkingDll: PARKING_inicio");
		return -1;
	}

	PARKING_fin = (int(*)(void)) GetProcAddress(controladorDll, "PARKING2_fin");
	if (PARKING_fin == NULL) {
		PERROR("LoadParkingDll: PARKING_fin");
		return -1;
	}

	PARKING_aparcar = (int(*)(HCoche, void *datos, TIPO_FUNCION_APARCAR_COMMIT, TIPO_FUNCION_PERMISO_AVANCE, TIPO_FUNCION_PERMISO_AVANCE_COMMIT)) GetProcAddress(controladorDll, "PARKING2_aparcar");
	if (PARKING_aparcar == NULL) {
		PERROR("LoadParkingDll: PARKING_aparcar");
		return -1;
	}

	PARKING_desaparcar = (int(*)(HCoche, void *datos, TIPO_FUNCION_PERMISO_AVANCE, TIPO_FUNCION_PERMISO_AVANCE_COMMIT)) GetProcAddress(controladorDll, "PARKING2_desaparcar");
	if (PARKING_desaparcar == NULL) {
		PERROR("LoadParkingDll: PARKING_desaparcar");
		return -1;
	}

	PARKING_getNUmero = (int(*)(HCoche)) GetProcAddress(controladorDll, "PARKING2_getNUmero");
	if (PARKING_getNUmero == NULL) {
		PERROR("LoadParkingDll: PARKING_getNUmero");
		return -1;
	}

	PARKING_getLongitud = (int(*)(HCoche)) GetProcAddress(controladorDll, "PARKING2_getLongitud");
	if (PARKING_getLongitud == NULL) {
		PERROR("LoadParkingDll: PARKING_getLongitud");
		return -1;
	}

	PARKING_getPosiciOnEnAcera = (int(*)(HCoche)) GetProcAddress(controladorDll, "PARKING2_getPosiciOnEnAcera");
	if (PARKING_getPosiciOnEnAcera == NULL) {
		PERROR("LoadParkingDll: PARKING_getPosiciOnEnAcera");
		return -1;
	}

	PARKING_getTServ = (unsigned long(*)(HCoche)) GetProcAddress(controladorDll, "PARKING2_getTServ");
	if (PARKING_getTServ == NULL) {
		PERROR("LoadParkingDll: PARKING_getTServ");
		return -1;
	}

	PARKING_getColor = (int(*)(HCoche)) GetProcAddress(controladorDll, "PARKING2_getColor");
	if (PARKING_getColor == NULL) {
		PERROR("LoadParkingDll: PARKING_getColor");
		return -1;
	}

	PARKING_getDatos = (void * (*)(HCoche)) GetProcAddress(controladorDll, "PARKING2_getDatos");
	if (PARKING_getDatos == NULL) {
		PERROR("LoadParkingDll: PARKING_getDatos");
		return -1;
	}

	PARKING_getX = (int(*)(HCoche)) GetProcAddress(controladorDll, "PARKING2_getX");
	if (PARKING_getX == NULL) {
		PERROR("LoadParkingDll: PARKING_getX");
		return -1;
	}

	PARKING_getY = (int(*)(HCoche)) GetProcAddress(controladorDll, "PARKING2_getY");
	if (PARKING_getY == NULL) {
		PERROR("LoadParkingDll: PARKING_getY");
		return -1;
	}

	PARKING_getX2 = (int(*)(HCoche)) GetProcAddress(controladorDll, "PARKING2_getX2");
	if (PARKING_getX2 == NULL) {
		PERROR("LoadParkingDll: PARKING_getX2");
		return -1;
	}

	PARKING_getY2 = (int(*)(HCoche)) GetProcAddress(controladorDll, "PARKING2_getY2");
	if (PARKING_getY2 == NULL) {
		PERROR("LoadParkingDll: PARKING_getY2");
		return -1;
	}

	PARKING_getAlgoritmo = (int(*)(HCoche)) GetProcAddress(controladorDll, "PARKING2_getAlgoritmo");;
	if (PARKING_getAlgoritmo == NULL) {
		PERROR("LoadParkingDll: PARKING_getAlgoritmo");
		return -1;
	}

	PARKING_isAceraOcupada = (int(*)(int algoritmo, int pos)) GetProcAddress(controladorDll, "PARKING2_isAceraOcupada");
	if (PARKING_isAceraOcupada == NULL) {
		PERROR("LoadParkingDll: PARKING_isAceraOcupada");
		return -1;
	}

	return 0;
}

int CreateIPC(void)
{
	int i, j;
	HANDLE hAuxHandle;

	/* Creacion carretera de mutex*/
	for (i = 0; i < N_AJUSTES; ++i) {
		for (j = 0; j < ROAD_LENGTH; ++j) {
			hAuxHandle = CreateSemaphore(NULL, 1, 1, NULL);
			if (hAuxHandle == NULL) {
				PERROR("CreateIPC: CreateSemaphore: 1");
				return -1;
			}
			IpcResources.hRoadSemaphores[i][j] = hAuxHandle;
		}
	}


	/* Inicialización de las aceras */
	for (i = 0; i < N_AJUSTES; ++i) {
		for (j = 0; j < ROAD_LENGTH; ++j) {
			IpcResources.nLotsArray[i][j] = EMPTY;
		}
	}

#ifdef ARRAY_SYNC
	/* Creacion array bidimensional de semaforos para controlar el orden de los coches*/
	for (j = 0; j < N_AJUSTES; j++) {

		hAuxHandle = CreateSemaphore(NULL, 1, 1, NULL);

		if (hAuxHandle == NULL) {
			PERROR("CreateIPC: CreateSemaphore: 2_1");
			return -1;
		}

		IpcResources.aTenThousandSemaphoresArray[j][0] = hAuxHandle;

		for (i = 1; i < TEN_THOUSAND; ++i) {
			hAuxHandle = CreateSemaphore(NULL, 0, 1, NULL);
			if (hAuxHandle == NULL) {
				PERROR("CreateIPC: CreateSemaphore: 2_2");
				return -1;
			}

			IpcResources.aTenThousandSemaphoresArray[j][i] = hAuxHandle;
		}
	}

#elif //LOOPING_SYNC

	/* Creación Mutex Ordenación Coches */
	for (i = 0; i < N_AJUSTES; i++){
		IpcResources.hMutexOrdenCoches[i] = CreateMutex(NULL, 0, NULL);
		if (IpcResources.hMutexOrdenCoches == NULL) {
			PERROR("CreateIPC: CreateMutex: hMutexOrdenCoches");
			return -1;
		}
	}


	/* Creación Evento Orden Coches */
	for (i = 0; i < N_AJUSTES; i++) {
		IpcResources.hEventOrdenCoches[i] = CreateEvent(NULL, 1, 1, NULL);
		if (IpcResources.hEventOrdenCoches == NULL) {
			PERROR("CreateIPC: CreateEvent: hEventOrdenCoches");
			return -1;
		}
	}

	for (i = 0; i < N_AJUSTES; i++) {
		IpcResources.nNextCar[i] = 1;
	}

#endif

	return 0;
}

int CloseIPC(void) {
	int i, j;

	/* Cierre de los handle de la carretera de mutex*/
	for (i = 0; i < N_AJUSTES; ++i) {
		for (j = 0; j < ROAD_LENGTH; ++j) {
			if (CloseHandle(IpcResources.hRoadSemaphores[i][j]) == 0) {
				PERROR("CloseIPC: CloseHandle: hRoadSemaphores");
				return -1;
			}
		}
	}

#ifdef ARRAY_SYNC
	/* Creacion array bidimensional de semaforos para controlar el orden de los coches*/
	for (j = 0; j < N_AJUSTES; j++) {
		for (i = 0; i < TEN_THOUSAND; ++i) {
			if (CloseHandle(IpcResources.aTenThousandSemaphoresArray[j][i]) == 0) {
				PERROR("CloseIPC: CloseHandle: aTenThousandSemaphoresArray");
				return -1;
			}
		}
	}
#endif
	return 0;
}

int TestArgs(int argc, char * argv[])
{
	int i,  debug_flag = 0;
	size_t j, length;

	/* Can receive between 2 and 3 arguments. Remember that argv[0] is file name
	* so limits are 3 and 4
	*/

	switch (argc) {

	case 3:
		if (strcmp(argv[2], "D") && strcmp(argv[2], "d")) {
			InvalidOptionMsg(argv[2]);
			return -1;
		}
		else {
			debug_flag = 1;
			length = strlen(argv[1]);
			for (j = 0; j < length; ++j) {
				if (!isdigit(argv[1][j])) {
					InvalidOptionMsg(argv[1]);
					return -1;
				}
			}
			break;
		}

	case 2:
		if (!strcmp(argv[1], "--help")) {
			HelpMsg();
			return -1;
		}

		length = strlen(argv[1]);
		for (j = 0; j < length; ++j) {
			if (!isdigit(argv[1][j])) {
				InvalidOptionMsg(argv[1]);
				return -1;
			}
		}
		break;

	default:
		ShortHelpMsg();
		return -1;
	}

	return debug_flag;
}

int InvalidOptionMsg(char * opt)
{
	char msg[] = "parking: invalid option -- '%s'\n\
Try 'parking --help' for more information.\n";

	return printf(msg, opt);
}

int HelpMsg(void)
{

	char msg[] = "Usage: parking SPEED [D]\n\
Simulates a process  allocation  system with  an  interface  that  emulates a\n\
road with cars which must be parked in a ordered fashion.\n\
    SPEED               Controls the speed of events. 0 is the  higher speed,\n\
                        and subsequent  incresing values, up to INT_MAX, slow\n\
                        down the simulation.\n\
    D                   A 'D' can be used  to  produce debugging  information\n\
                        about the simulation, which will be output on stderr.";

	return printf("%s\n", msg);

}

int ShortHelpMsg(void)
{

	char msg[1000] = "Usage: parking SPEED [D]\n\
Try 'parking --help' for more information.\n";

	return printf("%s", msg);

}

int LlegadaPrimerAjuste(HCoche hc)
{
#ifdef PRIMER_AJUSTE_OFF //__DEBUG__
	return -2;
#else

	int c_length;
	int i, j, pos;
	
	c_length = PARKING_getLongitud(hc);

	pos = -1;
	for (i = 0; i <= ROAD_LENGTH - c_length; ++i) {

		if (IpcResources.nLotsArray[PRIMER_AJUSTE][i] == EMPTY) {

			j = i;
			while (j < i + c_length) {
				if (EMPTY == IpcResources.nLotsArray[PRIMER_AJUSTE][j])
					++j;
				else
					break;
			}

			if (j == i + c_length) {

				for (j = i; j < i + c_length; j++) {
					IpcResources.nLotsArray[PRIMER_AJUSTE][j] = RESERVED;
				}

				pos =  i;
				break;
			}
		}
	}

	if (pos != -1) {

		if (NULL == CreateThread(NULL, 0, HiloAparcar, (LPVOID)hc, 0, NULL)) {
			PERROR("LlegadaPrimerAjuste: CreateThread: HiloAparcar");
			return -2;
		}
	}

	return pos;

#endif
}

int LlegadaSiguienteAjuste(HCoche hc)
{
#ifdef SIGUIENTE_AJUSTE_OFF //__DEBUG__
	return -2;
#else


	static int last_pos = -1;
	int c_length;
	int i, first_spot, free_spot;

	c_length = PARKING_getLongitud(hc);

	/* Se podría hacer con el módulo de la posición last_pos % ROAD_LENGTH,
	* pero creo que queda más complejo por el tratamiento especial al valor
	* de last_pos 0. Por ello, corrijo manualmente cuando last_pos se pasa
	* de ROAD_LENGTH -1
	*/

	if (last_pos > -1) {
		if (EMPTY != IpcResources.nLotsArray[SIGUIENTE_AJUSTE][last_pos]) {
			first_spot = SiguienteAjustePrimerHueco(last_pos);
			if (-1 == first_spot) {
				return -1;
			}
		}
		else {
			first_spot = last_pos;
			while (IpcResources.nLotsArray[SIGUIENTE_AJUSTE][first_spot] == EMPTY && first_spot>0) first_spot--;
		}
	}
	else {
		first_spot = 0;
	}

	free_spot = SiguienteAjusteAjustaPrimer(c_length,
		first_spot,
		ROAD_LENGTH);

	if (-1 == free_spot) {
		free_spot = SiguienteAjusteAjustaPrimer(c_length, 0,
			first_spot);
	}

	if (free_spot != -1) {

		for (i = free_spot; i < free_spot + c_length; ++i) {
			IpcResources.nLotsArray[SIGUIENTE_AJUSTE][i] = RESERVED;
		}

		last_pos = free_spot;

		if (NULL == CreateThread(NULL, 0, HiloAparcar, (LPVOID)hc, 0, NULL)) { 
			PERROR("LlegadaPrimerAjuste: CreateThread: HiloAparcar");
			return -2;
		}
	}

	return free_spot;

#endif

}

int SiguienteAjustePrimerHueco(int last_pos)
{
	int i;

	for (i = last_pos + 1; i < ROAD_LENGTH; ++i) {
		if (EMPTY == IpcResources.nLotsArray[SIGUIENTE_AJUSTE][i])
			return i;
	}

	for (i = 0; i < last_pos; ++i) {
		if (EMPTY == IpcResources.nLotsArray[SIGUIENTE_AJUSTE][i])
			return i;
	}

	return -1;
}

int SiguienteAjusteAjustaPrimer(int c_length, int a, int b)
{
	int i, j;

	for (i = a; i <= b - c_length; ++i) {

		if (IpcResources.nLotsArray[SIGUIENTE_AJUSTE][i] == EMPTY) {

			for (j = i; j < i + c_length; j++) {
				if (IpcResources.nLotsArray[SIGUIENTE_AJUSTE][j] != EMPTY)
					break;
			}

			if (j == i + c_length) {
				return i;
			}
		}
	}

	return -1;
}

int LlegadaMejorAjuste(HCoche hc)
{
#ifdef MEJOR_AJUSTE_OFF //__DEBUG__
	return -2;
#else

	int i, j;
	int c_length;
	int size, n_huecos, bestfit;
	int huecos[ROAD_LENGTH] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	};

	c_length = PARKING_getLongitud(hc);

	/* Buscamos los huecos en que el coche cabe */
	n_huecos = 0;
	i = 0;
	while (i <= ROAD_LENGTH - c_length) {

		size = 0;

		if (EMPTY == IpcResources.nLotsArray[MEJOR_AJUSTE][i]) {

			for (j = i; j < ROAD_LENGTH
				&& EMPTY == IpcResources.nLotsArray[MEJOR_AJUSTE][j]; ++j) {
				++size;
			}

			if (size >= c_length) {
				huecos[i] = size;
				++n_huecos;
				i += size;
			}
			else {
				++i;
			}

}
		else {
			++i;
		}
	}

	if (n_huecos < 1) {
		return -1;
	}

	/* Seleccionamos el hueco más pequeño */
	for (i = 0; i < ROAD_LENGTH; ++i) {
		if (huecos[i] != 0) {
			bestfit = i;
			break;
		}
	}

	for (i = bestfit + 1; i < ROAD_LENGTH; ++i) {
		if (huecos[i] != 0 && huecos[i] < huecos[bestfit]) {
			bestfit = i;
		}
	}

	for (i = bestfit; i < bestfit + c_length; ++i)
		IpcResources.nLotsArray[MEJOR_AJUSTE][i] = RESERVED;

	if (NULL == CreateThread(NULL, 0, HiloAparcar, (LPVOID)hc, 0, NULL)) { 
		PERROR("LlegadaPrimerAjuste: CreateThread: HiloAparcar");
		return -2;
	}
	

	return bestfit;


#endif

	// Crear hilo de aparcar
	// Devolver posición
}

int LlegadaPeorAjuste(HCoche hc)
{
#ifdef PEOR_AJUSTE_OFF //__DEBUG__
	return -2;
#else

	int i, j;
	int c_length;
	int size, n_huecos, worstfit;
	int huecos[ROAD_LENGTH] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	};

	c_length = PARKING_getLongitud(hc);

	/* Buscamos los huecos en que el coche cabe */
	n_huecos = 0;
	i = 0;
	while (i <= ROAD_LENGTH - c_length) {

		size = 0;

		if (EMPTY == IpcResources.nLotsArray[PEOR_AJUSTE][i]) {

			for (j = i; j < ROAD_LENGTH
				&& EMPTY == IpcResources.nLotsArray[PEOR_AJUSTE][j]; ++j) {
				++size;
			}

			if (size >= c_length) {
				huecos[i] = size;
				++n_huecos;
				i += size;
			}
			else {
				++i;
			}

		}
		else {
			++i;
		}
	}

	if (n_huecos < 1) {
		return -1;
	}

	/* Seleccionamos el hueco más grande */
	for (i = 0; i < ROAD_LENGTH; ++i) {
		if (huecos[i] != 0) {
			worstfit = i;
			break;
		}
	}

	for (i = worstfit + 1; i < ROAD_LENGTH; ++i) {
		if (huecos[i] != 0 && huecos[i] > huecos[worstfit]) {
			worstfit = i;
		}
	}

	for (i = worstfit; i < worstfit + c_length; ++i)
		IpcResources.nLotsArray[PEOR_AJUSTE][i] = RESERVED;

	if (NULL == CreateThread(NULL, 0, HiloAparcar, (LPVOID)hc, 0, NULL)) { 
		PERROR("LlegadaPrimerAjuste: CreateThread: HiloAparcar");
		return -2;
	}

	return worstfit;

#endif

	// Crear hilo de aparcar
	// Devolver posición
}

int SalidaPrimerAjuste(HCoche hc)
{
#if 0 //__DEBUG__
	return -2;
#else

	if (NULL == CreateThread(NULL, 0, HiloDesaparcar, (LPVOID)hc, 0, NULL)) { 
		PERROR("SalidaPrimerAjuste: CreateThread: HiloDesparcar");
		return -1;
	}

	return 0;

#endif
}

int SalidaSiguienteAjuste(HCoche hc)
{
#ifdef SIGUIENTE_AJUSTE_OFF //__DEBUG__
	return -2;
#else

	if (NULL == CreateThread(NULL, 0, HiloDesaparcar, (LPVOID)hc, 0, NULL)) { 
		PERROR("PrimerAjuste: CreateThread: HiloAparcar");
		return -1;
	}

	return 0;

#endif
}

int SalidaMejorAjuste(HCoche hc)
{
#ifdef MEJOR_AJUSTE_OFF //__DEBUG__
	return -2;
#else

	if (NULL == CreateThread(NULL, 0, HiloDesaparcar, (LPVOID)hc, 0, NULL)) { 
		PERROR("PrimerAjuste: CreateThread: HiloAparcar");
		return -1;
	}

	return 0;

#endif
}

int SalidaPeorAjuste(HCoche hc)
{
#ifdef PEOR_AJUSTE_OFF //__DEBUG__
	return -2;
#else

	if (NULL == CreateThread(NULL, 0, HiloDesaparcar, (LPVOID)hc, 0, NULL)) { 
		PERROR("PrimerAjuste: CreateThread: HiloAparcar");
		return -1;
	}

	return 0;

#endif
}

DWORD WINAPI HiloParkingInicio(LPVOID param)
{
	struct PARKING_inicio_args args;
	args = *(struct PARKING_inicio_args *) param;
	PARKING_inicio(args.aFuncionesLlegada, args.aFuncionesSalida, args.intervalo, args.d);
	return 0;
}

DWORD WINAPI HiloAparcar(LPVOID param)
{
	/* ORDEN DE LOS COCHES  */
	HCoche hc;
	int nNumeroCoche;
	int algoritmo;

	hc = (HCoche) param;
	nNumeroCoche = PARKING_getNUmero(hc);
	algoritmo = PARKING_getAlgoritmo(hc);

#ifdef ARRAY_SYNC

	if (WaitForSingleObject(
		IpcResources.aTenThousandSemaphoresArray[algoritmo][(nNumeroCoche - 1) % TEN_THOUSAND], INFINITE
	) == WAIT_FAILED) { // Condicion y espera atomicas
		PERROR("HiloAparcar: WaitForSingleObject");
	}

#else // LOOPING_SYNC
	if (WaitForSingleObject(IpcResources.hMutexOrdenCoches[algoritmo], INFINITE) == WAIT_FAILED) { // Condicion y espera atomicas
		PERROR("HiloAparcar: WaitForSingleObject");
	}

		while (nNumeroCoche != IpcResources.nNextCar[algoritmo]) {

			if (SignalObjectAndWait(IpcResources.hMutexOrdenCoches[algoritmo], IpcResources.hEventOrdenCoches[algoritmo], INFINITE, 0) == WAIT_FAILED) {
				PERROR("HiloAparcar: WaitForSingleObject");
			}

			if (WaitForSingleObject(IpcResources.hMutexOrdenCoches[algoritmo], INFINITE) == WAIT_FAILED) {
				PERROR("HiloAparcar: WaitForSingleObject");
			}

		}

	if (ReleaseMutex(IpcResources.hMutexOrdenCoches[algoritmo]) == 0) {
		PERROR("HiloAparcar: ReleaseMutex");
	}
#endif

	PARKING_aparcar(hc, NULL, AparcarCommit, PermisoAvance, PermisoAvanceCommit);

	return 0;
}

DWORD WINAPI HiloDesaparcar(LPVOID param)
{
	HCoche hc;

	hc = (HCoche)param;
   
	PARKING_desaparcar(hc, NULL, PermisoAvance, PermisoAvanceCommit);

	return 0;
}

void AparcarCommit(HCoche hc)
{
	int algoritmo = PARKING_getAlgoritmo(hc);
	int nNumeroCoche = PARKING_getNUmero(hc);

#ifdef ARRAY_SYNC

	if (ReleaseSemaphore(IpcResources.aTenThousandSemaphoresArray[algoritmo][nNumeroCoche % TEN_THOUSAND], 1, NULL) == 0){
		PERROR("AparcarCommit: ReleaseSemaphore");
	}
	
#else

	if (WaitForSingleObject(IpcResources.hMutexOrdenCoches[algoritmo], INFINITE) == WAIT_FAILED) { // Condicion y espera atomicas
		PERROR("AparcarCommit: WaitForSingleObject");
	}

	IpcResources.nNextCar[algoritmo] = nNumeroCoche + 1;

	if (PulseEvent(IpcResources.hEventOrdenCoches[algoritmo]) == 0) {
		PERROR("AparcarCommit: PulseEvent");
	}

	if (ReleaseMutex(IpcResources.hMutexOrdenCoches[algoritmo]) == 0) {
		PERROR("AparcarCommit: ReleaseMutex");
	}
#endif
}

void PermisoAvance(HCoche hc)
{
	int i;
	int X_inicio, X_fin, Y_inicio, Y_fin, algoritmo, c_length;

	X_inicio = PARKING_getX(hc);
	X_fin = PARKING_getX2(hc);
	Y_inicio = PARKING_getY(hc);
	Y_fin = PARKING_getY2(hc);
	algoritmo = PARKING_getAlgoritmo(hc);
	c_length = PARKING_getLongitud(hc);

	if (X_fin >= 0
		&& X_fin < ROAD_LENGTH){
		if (Y_inicio == LANE
			&& Y_fin == LANE) { // Horizontal

			if (WaitForSingleObject(IpcResources.hRoadSemaphores[algoritmo][X_fin], INFINITE) == WAIT_FAILED) {
				PERROR("PermisoAvance: WaitForSingleObject 1");
			}
		}
		else if (STRIPE == Y_inicio && LANE == Y_fin) { // Desaparcando
			for (i = X_fin + c_length - 1; i >= X_fin; --i) {
				if (WaitForSingleObject(IpcResources.hRoadSemaphores[algoritmo][i], INFINITE) == WAIT_FAILED) {
					PERROR("PermisoAvance: WaitForSingleObject 2");
				}
			}

		}
	}
}
	
void PermisoAvanceCommit(HCoche hc)
{
	int i;
	int X_inicio, X_fin, Y_inicio, Y_fin, algoritmo, c_length;

	X_inicio = PARKING_getX2(hc);
	X_fin = PARKING_getX(hc);
	Y_inicio = PARKING_getY2(hc);
	Y_fin = PARKING_getY(hc);
	algoritmo = PARKING_getAlgoritmo(hc);
	c_length = PARKING_getLongitud(hc);

	if (X_inicio + c_length - 1 >= 0
		&& X_inicio + c_length - 1 < ROAD_LENGTH) {

		if (LANE == Y_inicio
			&& LANE == Y_fin) { // Horizontal

			if (ReleaseSemaphore(IpcResources.hRoadSemaphores[algoritmo][X_inicio + c_length - 1], 1, NULL) == 0) {
				PERROR("PermisoAvanceCommit: ReleaseSemaphore 1");
			}

		}
		else if (STRIPE == Y_inicio && LANE == Y_fin) {
			for (i = X_inicio + c_length - 1; i >= X_inicio; --i){
				IpcResources.nLotsArray[algoritmo][i] = EMPTY;
			}
		}
		else if (LANE == Y_inicio && STRIPE == Y_fin) {
			for (i = X_inicio + c_length - 1; i >= X_inicio; --i) {
				if (ReleaseSemaphore(IpcResources.hRoadSemaphores[algoritmo][i], 1, NULL) == 0) {
					PERROR("PermisoAvanceCommit: ReleaseSemaphore 2");
				}
			}
		}
	}
}

void printCarretera()
{
	int i, val;

	val = IpcResources.nLotsArray[PRIMER_AJUSTE][0];
	for (i = 0; i < ROAD_LENGTH; ++i) {
		if (val != IpcResources.nLotsArray[PRIMER_AJUSTE][i]) {
			val = IpcResources.nLotsArray[PRIMER_AJUSTE][i];
			putchar(' ');
		}

		printf("%d", IpcResources.nLotsArray[PRIMER_AJUSTE][i]);
	}
	putchar('\n');
}



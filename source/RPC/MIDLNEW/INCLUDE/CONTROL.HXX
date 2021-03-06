/*****************************************************************************/
/**						Microsoft LAN Manager								**/
/**				Copyright(c) Microsoft Corp., 1987-1990						**/
/*****************************************************************************/
/*****************************************************************************
File				: control.hxx
Title				: compiler controller object
History				:
	05-Aug-2000	VibhasC	Created

*****************************************************************************/

#ifndef __CONTROL_HXX__
#define __CONTROL_HXX__

#include "listhndl.hxx"

#define IDL_PASS	(1)
#define ACF_PASS	(2)

/***
 *** The pass 1 controller.
 ***/

typedef class _pass1
	{
private:

	class idict			*	pFDeclRegistry;

public:

	STATUS_T				Go();
							_pass1();
							~_pass1() { };

	void					RegisterOneFDeclDef( class node_forward * );

	void					ResolveFDecl();

	} PASS_1;

/***
 *** The pass 2 controller.
 ***/

typedef class _pass2
	{
private:
	class node_interface*	pInterfaceNode;
	class type_node_list*	pAcfIncludeList;
public:
	STATUS_T				Go();
							_pass2();
							~_pass2() { };

	void					InsertAcfIncludeFile( class node_file *pFile )
								{
								pAcfIncludeList->SetPeer(
										( class node_skl *)pFile );
								}

	node_interface		*	GetInterfaceNode()
								{
								return pInterfaceNode;
								}

	} PASS_2;

/***
 *** The pass 3 controller.
 ***/

typedef class _pass3
	{
public:
	STATUS_T				Go();
							_pass3() { };
							~_pass3() { };
	} PASS_3;

/***
 *** The pass 4 controller.
 ***/

typedef class _pass4
	{
public:
	STATUS_T				Go();
							_pass4() { };
							~_pass4() { };
	} PASS_4;

/***
 *** The compiler controller class
 ***/

typedef class ccontrol
	{

private:
	short					PassNumber;
	class attrdict		*	pAttrDict;
	class _cmd_arg		*	pCommandProcessor;
	class _nfa_info		*	pImportController;
	PASS_1				*	pPass1Controller;
	PASS_2				*	pPass2Controller;
	PASS_3				*	pPass3Controller;
	PASS_4				*	pPass4Controller;
	short					ErrorCount;


public:


// member functions

	class _cmd_arg		*	GetCommandProcessor()
								{
								return pCommandProcessor;
								};
	class _cmd_arg		*	SetCommandProcessor( class _cmd_arg *p )
								{
								return pCommandProcessor = p;
								};

	class _nfa_info		*	GetImportController()
								{
								return pImportController;
								}
	class _nfa_info		*	SetImportController( class _nfa_info  * p )
								{
								return pImportController = p;
								}

	STATUS_T				Go();

							ccontrol( int, char *[]);
							~ccontrol();

	void					IncrementErrorCount()
								{
								ErrorCount++;
								}
	short					GetErrorCount()
								{
								return ErrorCount;
								};

	void					SetPassNumber( short PN )
								{
								PassNumber	= PN;
								}
	short					GetPassNumber()
								{
								return PassNumber;
								}

	} CCONTROL;

#endif // __CONTROL_HXX__

#include "stdAFX.h"
#include "CEconomy.h"
#include "IO_STB.h"

#ifdef	__SERVER
	#include "ZoneLIST.H"
#else
	short g_nWorldRate=100;
	short g_nWorldProc=100;

	short Get_WorldRATE ()						{	return g_nWorldRate;		}
	short Get_WorldPROD ()						{	return g_nWorldProc;		}

	void  Set_WorldRATE (short nWorldRate)		{	g_nWorldRate=nWorldRate;	}
	void  Set_WorldPROD (short nWorldProd)		{	g_nWorldProc=nWorldProd;	}
#endif

//-------------------------------------------------------------------------------------------------
CEconomy::CEconomy ()
{
	Init ();
}
CEconomy::~CEconomy ()
{
	// save ...
}

//-------------------------------------------------------------------------------------------------
void CEconomy::Init (void)
{
	// init !!!
	m_btTOWN_RATE  = 100;

#if	defined( __SERVER ) || defined( __VIRTUAL_SERVER )
	m_dwUpdateTIME = 0;
	m_dwCheckTIME = 0;

	m_btSave_TOWN_RATE = 0;
	::ZeroMemory( m_btSave_ItemRATE,	sizeof(BYTE)* MAX_PRICE_TYPE );
#endif

	for (short nP=MIN_PRICE_TYPE; nP<MAX_PRICE_TYPE; nP++) {
#if	defined( __SERVER ) || defined( __VIRTUAL_SERVER )
		m_iTownITEM[ nP ] = m_nTown_CONSUM[ nP ] * 100;
#endif
		m_btItemRATE[ nP ] = 50;
	}

	// load from db ...
}

bool CEconomy::Load (FILE *fp)
{
	int iTownCounter, iPopBase, iDevBase, iValue;

	Init ();

	fread( &iTownCounter,	sizeof(int),	1,	fp);	//	int	: 체크 카운터
	fread( &iPopBase,		sizeof(int),	1,	fp);	//  int	: 기준 인구
	fread( &iDevBase,		sizeof(int),	1,	fp);	//	int	: 기준 발전량

	if ( iTownCounter <= 1 )	
		iTownCounter = 60 * 24;

	m_iTownCounter = iTownCounter;
	#define	ECONMY_1MIN	( 1000*60 )			// 1 min
	m_dwTown_COUNTER = iTownCounter * ECONMY_1MIN;

	m_nTown_POP_BASE = iPopBase;
	m_nTown_DEV_BASE = iDevBase;

	m_iTownPOP = m_nTown_POP_BASE;
	m_nTownDEV = m_nTown_DEV_BASE;

	for (short nP=MIN_PRICE_TYPE; nP<MAX_PRICE_TYPE; nP++) 
	{
		fread( &iValue,	sizeof(int),	1,	fp);
		m_nTown_CONSUM[ nP ] = iValue;
	}

	return true;
}


#define	MIN_TOWN_ITEM	100
#define	MAX_TOWN_ITEM	32000

bool CEconomy::Proc (DWORD dwCurTIME)
{
	if ( dwCurTIME - m_dwCheckTIME < m_dwTown_COUNTER )
		return false;
	m_dwCheckTIME  = dwCurTIME;

	m_iTownPOP += (int)( ( m_nTownDEV - m_btTOWN_RATE ) * m_iTownPOP / 3000.f );

    short nP;
	for (nP=MIN_PRICE_TYPE; nP<MAX_PRICE_TYPE; nP++) {
		m_iTownITEM[ nP ] -= (int)( ( m_nTown_CONSUM[ nP ] * ( m_iTownCounter + 10 - m_nTownDEV*0.1f ) * m_iTownPOP ) / 50000 * ( m_iTownITEM[ nP ] * 0.01 + 50 ) / 100 );

		if ( m_iTownITEM[ nP ] < MIN_TOWN_ITEM ) m_iTownITEM[ nP ] = MIN_TOWN_ITEM;
		else
		if ( m_iTownITEM[ nP ] > MAX_TOWN_ITEM ) m_iTownITEM[ nP ] = MAX_TOWN_ITEM;
	}

	int iTotRATE=0, iNewVALUE;
	for (nP=MIN_PRICE_TYPE; nP<MAX_PRICE_TYPE; nP++) {
		iNewVALUE = m_btItemRATE[ nP ] + ( m_iTownPOP * m_nTown_CONSUM[ nP ] / 10 - m_iTownITEM[ nP ] ) / 600;

		if ( iNewVALUE < 45 ) 
			m_btItemRATE[ nP ] = 45;
		else
		if ( iNewVALUE > 65 )
			m_btItemRATE[ nP ] = 65;
		else
			m_btItemRATE[ nP ] = iNewVALUE;

		iTotRATE += m_btItemRATE[ nP ];
	}

	m_btTOWN_RATE = 80;

	m_nCur_WorldPROD = ::Get_WorldPROD ();
	m_nCur_WorldRATE = ::Get_WorldRATE ();

	if ( 0 == ::memcmp( m_btSave_DATA, m_btCur_DATA, sizeof( m_btSave_DATA ) ) )
		return false;

	m_dwUpdateTIME = dwCurTIME;
	::memcpy( m_btSave_DATA, m_btCur_DATA, sizeof( m_btSave_DATA ) );

	return true;
}


void CEconomy::BuyITEM (tagITEM &sITEM)
{
	// 생필품은 ITEM_TYPE_USE, ITEM_TYPE_NATURAL !!!
	switch( sITEM.GetTYPE() ) {
		case ITEM_TYPE_USE :
		case ITEM_TYPE_NATURAL :
		{
			short nRateTYPE = ITEM_RATE_TYPE( sITEM.GetTYPE(), sITEM.GetItemNO() );
			int iQuantity = ITEM_BASE_PRICE( sITEM.GetTYPE(), sITEM.GetItemNO() ) * sITEM.GetQuantity() / 100;

			m_iTownITEM[ nRateTYPE ] -= iQuantity;
			if ( m_iTownITEM[ nRateTYPE ] < MIN_TOWN_ITEM )
				m_iTownITEM[ nRateTYPE ] = MIN_TOWN_ITEM ;
		}
	}
}
void CEconomy::SellITEM (tagITEM &sITEM, int iQuantity)
{
	// 생필품은 ITEM_TYPE_USE, ITEM_TYPE_NATURAL !!!
	switch( sITEM.GetTYPE() ) {
		case ITEM_TYPE_USE :
		case ITEM_TYPE_NATURAL :
		{
			short nRateTYPE = ITEM_RATE_TYPE( sITEM.GetTYPE(), sITEM.GetItemNO() );
			
			m_iTownITEM[ nRateTYPE ] += ( ITEM_BASE_PRICE( sITEM.GetTYPE(), sITEM.GetItemNO() ) * iQuantity / 100 );
			if ( m_iTownITEM[ nRateTYPE ] > MAX_TOWN_ITEM )
				m_iTownITEM[ nRateTYPE ] = MAX_TOWN_ITEM;
		}
	}
}


//-------------------------------------------------------------------------------------------------
void CEconomy::SetTownRATE (int iValue)
{
	m_btTOWN_RATE = iValue;
}
void CEconomy::SetItemRATE(int iPriceType, int iValue)
{
	assert ( iPriceType >= MIN_PRICE_TYPE && iPriceType <  MAX_PRICE_TYPE );

	m_btItemRATE[ iPriceType ] = iValue;
}

//-------------------------------------------------------------------------------------------------
bool CEconomy::IsEssentialGoods (int iItemTYPE)
{
	// ITEM_TYPE( X,Y )
	if ( ( iItemTYPE >= 421 && iItemTYPE <= 428 ) ||
		 ( iItemTYPE >= 311 && iItemTYPE <= 312 ) )
		return true;

	return false;
}

//-------------------------------------------------------------------------------------------------
int CEconomy::Get_ItemBuyPRICE (short nItemTYPE, short nItemNO, short nBuySkillVALUE)
{
	int iPrice = 0;
	int iItemRate, iPriceRate;

	switch( nItemTYPE ) {
		case ITEM_TYPE_FACE_ITEM:  
		case ITEM_TYPE_HELMET	:  
		case ITEM_TYPE_ARMOR	:  
		case ITEM_TYPE_GAUNTLET :  
		case ITEM_TYPE_BOOTS	:  
		case ITEM_TYPE_KNAPSACK :  
		case ITEM_TYPE_WEAPON	:
		case ITEM_TYPE_SUBWPN	:
		case ITEM_TYPE_RIDE_PART:
			// 2004. 1. 2 구입 가격 = 
			iPrice = (int)(	ITEM_BASE_PRICE( nItemTYPE, nItemNO ) * ( ITEM_QUALITY( nItemTYPE, nItemNO )+50.f ) * ( 1.f - nBuySkillVALUE * 0.01 ) / 100.f + 0.5f );
			break;

		case ITEM_TYPE_USE		:
		case ITEM_TYPE_NATURAL	:
			if ( IsEssentialGoods( ITEM_TYPE( nItemTYPE, nItemNO ) ) ) {
				// 생필품.
				// 구입 가격 = ITEM_BASE * { 1 + (ITEM_RATE - 50) * ITEM_FLUC / 1000 } * (1 - 구입스킬레벨 * 0.02)
				iItemRate  = this->Get_ItemRATE( ITEM_RATE_TYPE( nItemTYPE, nItemNO ) );
				iPriceRate = ITEM_PRICE_RATE( nItemTYPE, nItemNO );

				iPrice = (int) (
							ITEM_BASE_PRICE( nItemTYPE, nItemNO ) * 
							( 1.f + ( iItemRate - 50.f ) * iPriceRate / 1000.f ) * ( 1.f - nBuySkillVALUE * 0.01f ) + 0.5f );
				break;
			}

		case ITEM_TYPE_JEWEL	:
		case ITEM_TYPE_GEM		:
		case ITEM_TYPE_QUEST	:
			// 2003. 08. 18 구입 가격 = ITEM_BASE * { 1 + (TOWN_RATE - 50) * ITEM_FLUC / 1000 } * (1 - 구입스킬레벨 * 0.02)
			iPriceRate = ITEM_PRICE_RATE( nItemTYPE, nItemNO );
			iPrice = (int) ( 
							ITEM_BASE_PRICE( nItemTYPE, nItemNO ) * 
							( 1.f + ( this->Get_TownRATE() - 50.f ) * iPriceRate / 1000.f ) * ( 1 - nBuySkillVALUE * 0.01f ) + 0.5f
							);
			break;
	}

	return iPrice;
}

//-------------------------------------------------------------------------------------------------
int CEconomy::Get_ItemSellPRICE (tagITEM &sITEM, short nSellSkillVALUE)
{
	int iPrice = 0;
	int iItemRate, iPriceRate;

	switch( sITEM.m_cType ) {
		case ITEM_TYPE_FACE_ITEM:  
		case ITEM_TYPE_HELMET	:  
		case ITEM_TYPE_ARMOR	:  
		case ITEM_TYPE_GAUNTLET :  
		case ITEM_TYPE_BOOTS	:  
		case ITEM_TYPE_KNAPSACK :  
		case ITEM_TYPE_WEAPON	:
		case ITEM_TYPE_SUBWPN	:
		case ITEM_TYPE_RIDE_PART:
			// 추가옵션은 보석과 같이
			iPrice = (int) ( 
						(float)ITEM_BASE_PRICE( sITEM.GetTYPE(), sITEM.GetItemNO() ) * 
						(float)( 40 + sITEM.GetGrade() ) * 
						(float)( 200 + sITEM.GetDurability() ) *
						(float)( 200 - Get_WorldRATE() ) * 
						(float)( 1 + nSellSkillVALUE * 0.01f ) / 1000000.f *
						(float)( (4000+sITEM.GetLife()) / 14000.f ) +
						(float)( sITEM.IsAppraisal() * GEMITEM_BASE_PRICE( sITEM.GetOption() ) * 0.2f )
					) ;
			break;

		case ITEM_TYPE_USE		:
		case ITEM_TYPE_NATURAL	:
			if ( IsEssentialGoods( ITEM_TYPE( sITEM.m_cType, sITEM.m_nItemNo ) ) ) {
				iItemRate  = this->Get_ItemRATE( ITEM_RATE_TYPE( sITEM.m_cType, sITEM.m_nItemNo ) );
				iPriceRate = ITEM_PRICE_RATE( sITEM.m_cType, sITEM.m_nItemNo );
				// 생필품.
				// 판매 가격 = ITEM_BASE * { 1000 + (ITEM_RATE - 50) * ITEM_FLUC } * (1 + 판매스킬레벨 * 0.02) * (200- WORLD_RATE) / 200000
				iPrice = (int) (
					ITEM_BASE_PRICE( sITEM.m_cType, sITEM.m_nItemNo) * 
					( 1000 + ( iItemRate-50 ) * iPriceRate ) *
					( 1 + nSellSkillVALUE * 0.01 ) * ( 200 - Get_WorldRATE() ) / 180000.f
					) ;
				break;
			}

		case ITEM_TYPE_JEWEL	:
		case ITEM_TYPE_GEM		:
		case ITEM_TYPE_QUEST	:
			// 2003.08.018 그 밖의 아이템.
			// 판매 가격 = ITEM_BASE * { 1000 + (TOWN_RATE - 50) * ITEM_FLUC } * (1 + 판매스킬레벨 * 0.02) * (200- WORLD_RATE) / 200000
			iPriceRate = ITEM_PRICE_RATE( sITEM.m_cType, sITEM.m_nItemNo );
			iPrice = (int) ( 
					ITEM_BASE_PRICE( sITEM.m_cType, sITEM.m_nItemNo) * 
					( 1000 + ( this->Get_TownRATE() - 50 ) * iPriceRate ) *
					( 1 + nSellSkillVALUE * 0.01 ) * ( 200 - Get_WorldRATE() ) / 180000.f
					) ;
			break;
	}

	return iPrice;
}

//-------------------------------------------------------------------------------------------------

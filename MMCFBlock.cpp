/*--------------------------------------------------------------------------*/
/*------------------------- File MMCFBlock.cpp ----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Implementation of the MMCFBlock class.
 *
 * \author Antonio Frangioni \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Enrico Gorgone \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \author Francesco Demelas \n
 *         Laboratoire d'Informatique de Paris Nord \n
 *         Universite' Sorbonne Paris Nord \n
 *
 * Copyright &copy by Antonio Frangioni, Enrico Gorgone, Francesco Demelas
 */
/*--------------------------------------------------------------------------*/
/*---------------------------- IMPLEMENTATION ------------------------------*/
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "MMCFBlock.h"

//#include <math.h>

#include <ctype.h>

/*--------------------------------------------------------------------------*/
/*------------------------- NAMESPACE AND USING ----------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*--------------------------------- TYPES ----------------------------------*/
/*--------------------------------------------------------------------------*/

using Index = Block::Index;

/*--------------------------------------------------------------------------*/
/*-------------------------------- MACROS ----------------------------------*/
/*--                                                                      --*/
/*--      Some small macro definitions, used throughout the code.         --*/
/*--                                                                      --*/
/*--------------------------------------------------------------------------*/

#define GOODN( n ) if( ( n <= 0 ) || ( Index( n ) > NNodes ) )	\
 throw( std::invalid_argument( "Invalid actual node name" ) )

#define GOODN2( n ) if( ( ( n <= 0 ) || ( Index( n ) > NNodes ) ) && \
			( n != -1 ) ) \
 throw( std::invalid_argument( "Invalid generic node name" ) )

#define GOODP( k ) if( ( k <= 0 ) || ( Index( k ) > NumProd ) ) \
 throw( std::invalid_argument( "Invalid actual product name" ) )

#define GOODP2( k ) if( ( ( k <= 0 ) || ( Index( k ) > NumProd ) ) && \
			( k != -1 ) ) \
 throw( std::invalid_argument( "Invalid generic product name" ) )

#define GOODL( l ) if( Index( l ) > NCnst ) \
 throw( std::invalid_argument( "Invalid link name" ) )

/*--------------------------------------------------------------------------*/
/*----------------------------- FUNCTIONS ----------------------------------*/
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/*--------------------------- STATIC MEMBERS -------------------------------*/
/*--------------------------------------------------------------------------*/

// register MMCFBlock to the Block factory
SMSpp_insert_in_factory_cpp_1( MMCFBlock );

/*--------------------------------------------------------------------------*/
/*------------------------ OTHER INITIALIZATIONS ---------------------------*/
/*--------------------------------------------------------------------------*/

void MMCFBlock::load( const std::string & input , char frmt )
{
 if( ! frmt )
  frmt = 'c';
 else
  frmt = tolower( frmt );

 if( ( frmt == 's' ) || ( frmt == 'c' ) ) {  // single-file formats
  Block::load( input , frmt );  // open stream and dispatch to load( stream )
  return;
  }

 // ensure starting from clean slate
 guts_of_destructor();
 
 // check parameters- - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 if( ( frmt != 'm' ) && ( frmt != 'p' ) && ( frmt != 'd' ) &&
     ( frmt != 'o' ) && ( frmt != 'u' ) )
  throw( std::invalid_argument( "invalid file type" +
				std::string( 1 , frmt ) ) );

 // reading general informations- - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 std::string fname = input + ".nod";
 std::ifstream inputS( fname );
 if( ! inputS.is_open() )
  throw( std::invalid_argument( "can't open file" + fname ) );

 inputS >> NComm;
 inputS >> NNodes;
 inputS >> NArcs;
 inputS >> NCnst;

 if( NNodes <= 1 )
  throw( std::invalid_argument( "wrong node number" ) );
 if( NArcs <= 0 )
  throw( std::invalid_argument( "wrong arc number" ) );
 if( NComm <= 0 )
  throw( std::invalid_argument( "wrong commodity number" ) );
 if( NCnst > NArcs )
  throw( std::invalid_argument( "wrong constraints number" ) );

 inputS.close();

 Subset Origins;
 Subset Destins;

 Subset StartOfK;
 Subset TempIdx;

 Index NumProd = NComm;

 // format-dependent part - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 if( frmt == 'u' )
  fname = input + ".od";
 else
  fname = input + ".sup";

 // determining the actual number of commodities for (OSP) or (ODS)- - - - - -
 // formulations: in the first case, a commodity is a pair ( product , - - - -
 // origin ), while in the second case it is a triplet ( product ,-  - - - - -
 // origin , destination ) - - - - - - - - - - - - - - - - - - - - - - - - - -

 if( ( frmt == 'd' ) || ( frmt == 'o' ) ) {
  StartOfK.resize( NumProd + 1 , 0 );

  inputS.clear();        // ensure failbits are not left dirty
  inputS.open( fname );  // commodities can be told from supplies
  if( ! inputS.is_open() )
   throw( std::invalid_argument( "can't open file" + fname ) );

  int origin;   // the *.sup file is read once here just to count the number
  int dest;     // of commodities: the actual data reading will be done later
  int comm;
  FNumber flow;

  if( frmt == 'd' )  // in (ODS) count the different O/D pairs - - - - - -
   while( inputS >> origin ) {
    GOODN( origin );

    inputS >> dest;
    GOODN( dest );

    inputS >> comm;
    GOODP2( comm );

    inputS >> flow;

    if( comm != -1 )
     StartOfK[ comm ]++;
    else
     for( Index i = NumProd ; i ; )
      StartOfK[ i-- ]++;
    }
  else  // in (OSP) count the number of different Origins- - - - - - - - - - -
   while( inputS >> origin ) {
    GOODN( origin );

    inputS >> dest;
    GOODN2( dest );

    inputS >> comm;
    GOODP2( comm );

    inputS >> flow;

    if( dest == -1 ) {
     if( comm != -1 )
      StartOfK[ comm ]++;
     else
      for( Index i = NumProd ; i ; )
       StartOfK[ i-- ]++;
     }
    }

  // really construct StartOfK - - - - - - - - - - - - - - - - - - - - - - - -

  StartOfK[ 0 ] = 0;
  for( Index i = 2 ; i <= NumProd ; i++ )
   StartOfK[ i ] += StartOfK[ i - 1 ];

  NComm = StartOfK[ NumProd ];  // note that NComm can "surprisingly" be
                                // < NumProd if some product does not appear

  Origins.resize( NComm );

  inputS.close();

  }  // end if( (OSP) or (ODP) )

 // allocating and initializing memory- - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 C.resize( NComm );  // allocate costs
 F.resize( NArcs );  // allocate fixed costs

 for( Index i = 0 ; i < NComm ; i++ )
  C[ i ].resize( NArcs , Inf< CNumber >() );  // arcs are un-existent
                                              // unless otherwise stated

 U.resize( NComm );  // allocate capacities - - - - - - - - - - - - -

 for( Index i = 0 ; i < NComm ; i++ )
  U[ i ].resize( NArcs , 0 );  // arcs are un-existent
                               // unless otherwise stated

 B.resize( NComm );  // allocate deficits

 for( Index i = 0 ; i < NComm ; i++ )
  B[ i ].resize( NNodes , 0 );    // nodes all have 0 deficit
                                  // unless otherwise stated

 // allocate start/end nodes and mutual capacities- - - - - - - - - - - - - -

 Startn.resize( NArcs );
 Endn.resize( NArcs );

 UTot.resize( NArcs );

 // reading supply/demand infos, or everything in one-files format- - - - - -
 // (this part is partly splitted again)- - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 if( frmt == 'm' )
  TempIdx.resize( NCnst );
 else {
  TempIdx.resize( std::max( Index( NumProd + 1 ) , NComm ) );
  if( ( frmt == 'o' ) || ( frmt == 'd' ) )
   std::copy( StartOfK.begin() , StartOfK.begin() + NumProd + 1 ,
	      TempIdx.begin() );
  }

 inputS.clear();        // ensure failbits are not left dirty
 inputS.open( fname );  // the right name is already there
 if( ! inputS.is_open() )
  throw( std::invalid_argument( "can't open file" + fname ) );

 switch( frmt ) {
 case( 'm' ): // mnetgen format - - - - - - - - - - - - - - - - - - - - - - -
 {            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  for( Index node ; inputS >> node ; ) {
   GOODN( node );

   int comm;
   inputS >> comm;
   GOODP2( comm );

   FNumber flow;
   inputS >> flow;

   if( comm == -1 )
    for( Index k = 0 ; k < NComm ; )
     B[ k++ ][ node - 1 ] = flow;
   else
    B[ comm - 1 ][ node - 1 ] = flow;
   }

  break;

  }  // end case( m ) - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 case( 'p' ): // JL (PSP) format- - - - - - - - - - - - - - - - - - - - - - -
 {            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  for( int origin ; inputS >> origin ; ) {
   GOODN2( origin );

   int dest;
   inputS >> dest;
   GOODN2( dest );
   if( ( ( origin == -1 ) && ( dest == -1 ) ) ||
       ( ( origin != -1 ) && ( dest != -1 ) ) )
    throw( std::invalid_argument( "exactly one p/d in PSP" ) );

   int comm;
   inputS >> comm;
   GOODP2( comm );

   FNumber flow;
   inputS >> flow;

   if( comm != -1 ) {
    comm--;

    if( origin < 0 )
     B[ comm ][ dest - 1 ] = -flow;
    else
     B[ comm ][ origin - 1 ] = flow;
    }
   else
    if( origin < 0 )
     for( Index i = NumProd ; i-- ; )
      B[ i ][ dest - 1 ] = -flow;
    else
     for( Index i = NumProd ; i-- ; )
      B[ i ][ origin - 1 ] = flow;

   }  // end( for( ! eof() ) )

  break;

  }  // end case( p ) - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 case( 'o' ): // JL (OSP) format- - - - - - - - - - - - - - - - - - - - - - -
 {            // while reading supplies, commodity "names" are assigned - - -

  for( int origin ; inputS >> origin ; ) {
   int dest;
   inputS >> dest;

   int comm;
   inputS >> comm;

   FNumber flow;
   inputS >> flow;

   if( comm != -1 ) {
    // origin or destination node for the given pair ( product , origin )

    Index i = StartOfK[ --comm ];

    while( ( i < TempIdx[ comm ] ) && ( Origins[ i ] != Index( origin ) ) )
     i++;  // seek the name of the commodity

    if( i == TempIdx[ comm ] ) {  // a "new" commodity
     Origins[ i ] = origin;
     TempIdx[ comm ]++;
     }

    if( dest == -1 )  // it is an origin
     B[ i ][ origin - 1 ] = flow;
    else
     B[ i ][ dest - 1 ] = - flow;
    }
   else {  // comm == -1
    // origin or destination node for all the commodities ( k , origin )
    // for each product k

    Index i;
    for( Index k = NumProd ; k-- ; ) {
     i = StartOfK[ k ];

     while( ( i < TempIdx[ k ] ) && ( Origins[ i ] != Index( origin ) ) )
      i++;  // seek the name of the commodity

     if( i == TempIdx[ k ] ) {  // a "new" commodity
      Origins[ i ] = origin;
      TempIdx[ i ]++;
      }

     if( dest == -1 )  // it is an origin
      B[ i ][ origin - 1 ] = flow;
     else
      B[ i ][ dest - 1 ] = - flow;

     }  // end for( k )
    }  // end else( comm == -1 )
   }  // end while( ! eof() )

  break;

  }  // end case( o ) - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 default: // 'd' == JL (ODS) format - - - - - - - - - - - - - - - - - - - - -
 {        // while reading supplies, commodity "names" are assigned - - - - -

  Destins.resize( NComm );

  for( int origin ; inputS >> origin ; ) {
   int dest;
   inputS >> dest;

   int comm;
   inputS >> comm;

   FNumber flow;
   inputS >> flow;

   if( comm != -1 ) {
    comm--;

    Origins[ TempIdx[ comm ] ] = origin;
    Destins[ TempIdx[ comm ] ] = dest;

    B[ TempIdx[ comm ] ][ dest - 1 ] = -flow;
    B[ TempIdx[ comm ]++ ][ origin - 1 ] = flow;
    }
   else
    for( Index i = NumProd ; i-- ; ) {
     Origins[ TempIdx[ i ] ] = origin;
     Destins[ TempIdx[ i ] ] = dest;

     B[ TempIdx[ i ] ][ dest - 1 ] = -flow;
     B[ TempIdx[ i ]++ ][ origin - 1 ] = flow;
     }
    }  // end while( ! eof() )
   }   // end default()- - - - - - - - - - - - - - - - - - - - - - - - - - - -
  }    // end switch( FT ) - - - - - - - - - - - - - - - - - - - - - - - - - -
       //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 inputS.close();

 UTot.assign( NArcs , Inf< FNumber >() );

 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // now the though part: reading arc info - - - - - - - - - - - - - - - - - -

 fname = input + ".arc";

 inputS.clear();        // ensure failbits are not left dirty
 inputS.open( fname );
 if( ! inputS.is_open() )
  throw( std::invalid_argument( "can't open file" + fname ) );

 if( frmt == 'm' ) {  // mnetgen format - - - - - - - - - - - - - - - - - - -
  Index who;  // it is dealt with separately, since it's simpler: the
              // name of the arc (who) is explicitly given

  while( inputS >> who ) {
   if( ( who <= 0 ) || ( who > NArcs ) )
    throw( std::invalid_argument( "invalid arc name" ) );
   who--;

   Index from;
   inputS >> from;
   GOODN( from );

   Index to;
   inputS >> to;
   GOODN( to );
   if( from == to )
    throw( std::invalid_argument( "self-loop" ) );

   int comm;
   inputS >> comm;
   GOODP2( comm );

   CNumber cost;
   inputS >> cost;

   FNumber cap;
   inputS >> cap;
   if( cap < 0 )
    cap = Inf< FNumber >();

   Index ptr;
   inputS >> ptr;
   GOODL( ptr );

   if( ptr )
    TempIdx[ ptr - 1 ] = who;

   Startn[ who ] = from;
   Endn[ who ] = to;

   if( comm == -1 )
    for( Index k = 0 ; k < NComm ; ) {
     C[ k ][ who ] = cost;
     U[ k++ ][ who ] = cap;
     }
   else {
    C[ --comm ][ who ] = cost;
    U[ comm ][ who ] = cap;
    }
   }   // end while()
  }    // end if( mnetgen )
 else {  // the three JL formats- - - - - - - - - - - - - - - - - - - - - - -
         // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  Index unbndld = 0;  // counter of unbundled links so far

  for( Index from ; inputS >> from ; ) {  // first, the usual data reading
   GOODN( from );

   Index to;
   inputS >> to;
   GOODN( to );
   if( from == to )
    throw( std::invalid_argument( "self-loop" ) );

   int comm;
   inputS >> comm;
   GOODP2( comm );

   CNumber cost;
   inputS >> cost;

   int cap;
   inputS >> cap;

   int origin;
   inputS >> origin;
   GOODN2( origin );

   int dest;
   inputS >> dest;
   GOODN2( dest );

   Index ptr;
   inputS >> ptr;
   GOODL( ptr );

   // now, the main part: from the triplet (origin, destination, product)
   // plus the file type (p, d, o) a list of applicable commodities is
   // constructed and put into TempIdx: then the arc will be replicated over
   // all the commodities of the list
   // in all the three fields, a "-1" takes the place of a wildcard

   Index TmpCommCntr = 1;

   switch( frmt ) {
    case( 'p' ):  // the simplest, only 2 subcases- - - - - - - - - - - - - -

     if( comm != -1 )      // a specific commodity (prod)
      TempIdx[ 0 ] = comm - 1;
     else                  // all commodities (!?)
      for( Index i = TmpCommCntr = NComm ; i-- ; )
       TempIdx[ i ] = i;

     break;

    case( 'o' ):  // tougher, 4 subcases- - - - - - - - - - - - - - - - - - -

     if( comm != -1 ) {
      Index i = StartOfK[ comm - 1 ];

      if( origin != -1 ) {          // a specific commodity ( prod , origin )
       while( ( i < StartOfK[ comm ] ) &&
	      ( Origins[ i ] != Index( origin ) ) )
        i++;

       assert( i != StartOfK[ comm ] );
       TempIdx[ 0 ] = i;
       }
      else                          // all commodities with a given product
       for( TmpCommCntr = 0 ; i < StartOfK[ comm ] ; )
        TempIdx[ TmpCommCntr++ ] = i++;
      }
     else
      if( origin != -1 )            // all commodities with a given origin
       for( Index i = TmpCommCntr = 0 ; i < NumProd ; ) {
        Index k = StartOfK[ i++ ];

        while( ( k < StartOfK[ i ] ) &&
	       ( Origins[ k ] != Index( origin ) ) )
         k++;

        if( k < StartOfK[ comm ] )
         TempIdx[ TmpCommCntr++ ] = k;
        }
      else                         // all commodities
       for( Index i = TmpCommCntr = NComm ; i-- ; )
        TempIdx[ i ] = i;

     break;

    default:     // == 'd', the toughest: 8 subcases- - - - - - - - - - - - -

     if( comm != -1 ) {
      Index i = StartOfK[ comm - 1 ];

      if( dest != -1 ) {
       if( origin != -1 ) {      // a specific commodity (prod, origin, dest)
        while( ( i < StartOfK[ comm ] ) && ( Destins[ i ] != Index( dest ) )
               && ( Origins[ i ] != Index( origin ) ) )
         i++;

        assert( i != StartOfK[ comm ] );
        TempIdx[ 0 ] = i;
        }
       else                      // all commodities with a given (prod, dest)
        for( TmpCommCntr = 0 ; i < StartOfK[ comm ] ; i++ )
         if( Destins[ i ] == Index( dest ) )
          TempIdx[ TmpCommCntr++ ] = i;
       }
      else {                     // dest == -1
       if( origin != -1 ) {      // all commodities with a given (prod, orig)
        for( TmpCommCntr = 0 ; i < StartOfK[ comm ] ; i++ )
         if( Origins[ i ] == Index( origin ) )
          TempIdx[ TmpCommCntr++ ] = i;
        }
       else                      // all commodities with a given (prod)
        for( TmpCommCntr = 0 ; i < StartOfK[ comm ] ; i++ )
         TempIdx[ TmpCommCntr++ ] = i;
       }
      }
     else                       // comm == -1
      if( dest != -1 ) {
       if( origin != -1 ) {     // all commodities with a given (orig, dest)
        for( Index i = TmpCommCntr = 0 ; i < NComm ; i++ )
         if( ( Destins[ i ] == Index( dest ) ) &&
	     ( Origins[ i ] == Index( origin ) ) )
          TempIdx[ TmpCommCntr++ ] = i;
        }
       else                    // all commodities with a given (dest)
        for( Index i = TmpCommCntr = 0 ; i < NComm ; i++ )
         if( Destins[ i ] == Index( dest ) )
          TempIdx[ TmpCommCntr++ ] = i;
       }
     else                      // dest == -1
      if( origin != -1 ) {     // all commodities with a given (origin)
       for( Index i = TmpCommCntr = 0 ; i < NComm ; i++ )
        if( Origins[ i ] == Index( origin ) )
         TempIdx[ TmpCommCntr++ ] = i;
        }
      else                     // all commodities
       for( Index i = TmpCommCntr = NComm ; i-- ; )
        TempIdx[ i ] = i;

    }  // end switch( FT )

   // now "filling" the proper arc for each commodity

   for( Index i = TmpCommCntr ; i-- ; ) {
    comm = TempIdx[ i ];
    Index who;

    if( ptr ) {      // if ptr != 0 it's easy
     who = ptr - 1;  // ( ptr - 1 ) is already the correct name

     Startn[ who ] = from;
     Endn[ who ] = to;
     }
    else {           // otherwise find the "name" of arc (from, to)
     Index k = 0;    // and put it into who

     while( ( k < unbndld ) &&
	    ( ( Startn[ NCnst + k ] != from ) ||
	      ( Endn[ NCnst + k ] != to ) ||
	      ( C[ comm ][ NCnst + k ] < Inf< CNumber >() ) ) )
      k++;

     // search for an arc (from, to) already defined among the unbundled ones
     // and whose "instance" relative to commodity comm has not already been
     // taken: this is not the only way of accommodating unbundled arcs, (in
     // case of multiple instances of an unbundled arc (i, j)), but it is
     // easy to see that all the resulting problems, however you distribute
     // the instances to arcs, are equivalent

     who = NCnst + k;
     assert( who < NArcs );

     if( k == unbndld ) {  // if no such arc exists ...
      unbndld++;           // ... a new one is created
      Startn[ who ] = from;
      Endn[ who ] = to;
      }
     }  // end else( ! ptr )

    C[ comm ][ who ] = cost;
    U[ comm ][ who ] = ( cap >= 0 ? cap : Inf< FNumber >() );

    } // end for( all comm. )
   }  // end while( ! eof() )

  if( NCnst + unbndld < NArcs )
   NArcs = NCnst + unbndld;

  }   // end else( JL formats )

 inputS.close();

 // reading mutual capacities - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 fname = input + ".mut";
 inputS.clear();        // ensure failbits are not left dirty
 inputS.open( fname );
 if( ! inputS.is_open() )
  throw( std::invalid_argument( "can't open file" + fname ) );

 for( Index i = 0 ; i < NCnst ; ) {
  Index j;
  inputS >> j;

  FNumber f;
  inputS >> f;

  if( frmt == 'm' )
   j = TempIdx[ i++ ];
  else
   j = i++;

  UTot[ j ] = ( f >= 0 ? f : Inf< FNumber >() );
  }

 // common initializations- - - - - - - - - - - - - - - - - - - - - - - - - -
 CmnIntlz();

 // issue Modification- - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // note: this is a NBModification, the "nuclear option"

 if( anyone_there() )
  add_Modification( std::make_shared< NBModification >( this ) );

 }  // end( MMCFBlock::load( std::string ) )

/*--------------------------------------------------------------------------*/

void MMCFBlock::load( std::istream & input , char frmt )
{
 if( ! frmt )
  frmt = 'c';
 else
  frmt = tolower( frmt );

 if( ( frmt != 's' ) && ( frmt != 'c' ) )
  throw( std::invalid_argument( "MMCFBlock::load: unsupported format" ) );

 // ensure starting from clean slate
 guts_of_destructor();

 // read dimensions - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 input >> NNodes;
 input >> NArcs;
 input >> NComm;

 if( frmt == 'c' ) {  // in the PPRN format, read description of side
  input >> NXtrC;     // constraints and prepare the data structures

  Index NNZ;
  input >> NNZ;

  if( NNZ ) {
   IdxBeg.resize( NXtrC );
   CoefIdx.resize( NNZ );
   CoefVal.resize( NNZ );
   }
  }

 NCnst = NArcs;

 if( NNodes <= 1 )
  throw( std::invalid_argument( "wrong node number" ) );
 if( NArcs <= 0 )
  throw( std::invalid_argument( "wrong arc number" ) );
 if( NComm <= 0 )
  throw( std::invalid_argument( "wrong commodity number" ) );

 // allocating and initializing memory- - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 C.resize( NComm );  // allocate costs
 F.resize( NArcs );  // allocate fixed costs

 if( frmt == 'c' )
  for( Index i = 0 ; i < NComm ; i++ )
   C[ i ].resize( NArcs );
 else
  for( Index i = 0 ; i < NComm ; i++ )
   C[ i ].resize( NArcs , Inf< CNumber >() );  // arcs are un-existent
                                               // unless otherwise stated

 U.resize( NComm );  // allocate capacities - - - - - - - - - - - - -

 if( frmt == 'c' )
  for( Index i = 0 ; i < NComm ; i++ )
   U[ i ].resize( NArcs );
 else
  for( Index i = 0 ; i < NComm ; i++ )
   U[ i ].resize( NArcs , 0 );  // arcs are un-existent
                                // unless otherwise stated

 B.resize( NComm );  // allocate deficits

 if( frmt == 'c' )
  for( Index i = 0 ; i < NComm ; i++ )
   B[ i ].resize( NNodes );
 else
  for( Index i = 0 ; i < NComm ; i++ )
   B[ i ].resize( NNodes , 0 );  // nodes all have 0 deficit
                                 // unless otherwise stated

 // allocate start/end nodes and mutual capacities- - - - - - - - - - - - - -

 Startn.resize( NArcs );
 Endn.resize( NArcs );

 UTot.resize( NArcs );

 c_Index NumProd = NComm;
 
 // reading the stuff - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 switch( frmt ) {
 case( 's' ): // Canadian format- - - - - - - - - - - - - - - - - - - - - - -
 {            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  // allocate the data structures for "extra" things- - - - - - - - - - - - -

  F.resize( NArcs );

  for( Index i = 0 ; i < NArcs ; i++ ) {  // read arc-related info- - - - - -
   input >> Endn[ i ];
   GOODN( Endn[ i ] );

   input >> Startn[ i ];
   GOODN( Startn[ i ] );
   if( Startn[ i ] == Endn[ i ] )
    throw( std::invalid_argument( "self-loop" ) );

   input >> F[ i ];
   
   FNumber f;
   input >> f;

   UTot[ i ] = ( f >= 0 ? f : Inf< FNumber >() );

   Index h;
   input >> h;

   for( ; h-- ; ) {
    Index k;
    input >> k;
    GOODP( k );

    input >> C[ --k ][ i ];
    input >> f;

    U[ k ][ i ] = ( f >= 0 ? f : Inf< FNumber >() );

    }  // end for( h )
   }  // end for( i )

  for( Index k ; input >> k ; ) {  // read node-related info - - - - - - - -
   GOODP( k );

   Index i;
   input >> i;
   GOODN( i );

   FNumber f;
   input >> f;
   B[ --k ][ --i ] = -f;
   }

  break;

  }  // end case( s ) - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 case( 'c' ): // PPRN format- - - - - - - - - - - - - - - - - - - - - - - - -
 {            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  for( Index k = 0 ; k < NComm ; k++ )  // read all costs
   for( Index i = 0 ; i < NArcs ; )
    input >> C[ k ][ i++ ];

  for( Index k = 0 ; k < NComm ; k++ )  // read all capacities
   for( Index i = 0 ; i < NArcs ; ) {
    FNumber f;
    input >> f;
    U[ k ][ i++ ] = ( f >= 0 ? f : Inf< FNumber >() );
    }

  for( Index k = 0 ; k < NComm ; k++ )  // read all supplies
   for( Index i = 0 ; i < NNodes ; ) {
    FNumber f;
    input >> f;
    B[ k ][ i++ ] = -f;
    }

  for( Index i = 0 ; i < NArcs ; ) {      // read all total capacities
   FNumber f;
   input >> f;
   UTot[ i++ ] = ( f >= 0 ? f : Inf< FNumber >() );
   }

  for( Index i = 0 ; i < NArcs ; i++ ) {  // read graph topology
   input >> Startn[ i ];
   GOODN( Startn[ i ] );

   input >> Endn[ i ];
   GOODN( Endn[ i ] );

   if( Startn[ i ] == Endn[ i ] )
    throw( std::invalid_argument( "self-loop" ) );
   }

  if( NXtrC ) {  // if there are "extra" constraints- - - - - - - - - - - - -
   throw( std::logic_error(
		      "extra constraints in PPRN format not managed yet" ) );

   // TODO: properly implement this, B[] is NComm long and this code is
   //       no longer working
   B[ NComm ].resize( NXtrC );
   B[ NComm + 1 ].resize( NXtrC );

   for( Index i = 0 ; i < NXtrC ; ) {  // read "extra" Uppr./Lwr. bounds
    input >> B[ NComm ][ i ];
    input >> B[ NComm + 1 ][ i++ ];
    }

   Index currc = 0;
   Index currpos = 0;
   for( Index j ; input >> j ; ) {  // read extra constraints description:
    Index k;                         // j = arc name
    input >> k;                     // commodity name

    CoefIdx[ currpos ] = (--k) * NArcs + (--j);

    Index h;
    input >> h;                     // constraint name
    h--;

    while( h > currc ) {
     IdxBeg[ currc ] = currpos;
     currc++;
     }

    input >> CoefVal[ currpos++ ];    // the coefficient

    }  // end( for( ! eof() ) )

   IdxBeg[ currc ] = currpos;

   }  // end( if( extra constraints ) )
  }  // end case( c ) - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 }   // end switch( FT )- - - - - - - - - - - - - - - - - - - - - - - - - - -
     // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 // common initializations- - - - - - - - - - - - - - - - - - - - - - - - - -
 CmnIntlz();

 // issue Modification- - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // note: this is a NBModification, the "nuclear option"

 if( anyone_there() )
  add_Modification( std::make_shared< NBModification >( this ) );

 }  // end( MMCFBlock::load( std::istream ) )

/*--------------------------------------------------------------------------*/

void MMCFBlock::generate_abstract_variables( Configuration * stvv )
{
 if( AR & HasVar ) {
  // TODO: check if stvv agrees with the formulation we currently have
  //       and throw exception otherwise
  return;
  }

 // TODO: check stvv and construct other formulations accordingly
 unsigned char fr = 0;
 auto c = dynamic_cast< SimpleConfiguration< int > * >( stvv );
 if( ( ! c ) && f_BlockConfig &&
     f_BlockConfig->f_static_variables_Configuration )
  c = dynamic_cast< SimpleConfiguration< int > * >(
                        f_BlockConfig->f_static_variables_Configuration );
 if( c )
  fr = c->value();
 
 AR = ( AR & ( ~4 ) ) | ( 4 * fr );
  
 // initialize the children - - - - - - - - - - - - - - - - - - - - - - - - -

 if( ! ( AR & KnapsackRelaxation ) ) {
  v_Block.resize( NComm );
 
  for( Index k = 0 ; k < NComm ; ++k ) {
   //!! TODO: if( PT[ k ] == kSPT ) do something more clever
   auto MCFb = new MCFBlock( this );
   MCFb->load( NNodes , NArcs , Startn , Endn , U[ k ] , C[ k ] , B[ k ] );
   v_Block[ k ] = MCFb;
   }
  }
 else {
  //construct vectors for the flow relaxation
  int items;
  std::vector< double > bound;
  std::vector< bool > Integrality;
  FMultiVector weights;
  FMultiVector costs;
  double Cmax = 0;
  double Umax = 0;
  double sumF = 0;
  int sumQ = 0;

   int i = 0;
   
   for( int j = 0 ; j < NNodes ; j++ )
    for( int k = 0 ; k < NComm ; k++ ) {
     if( B[ k ][ j ] > 0 )
      sumQ += B[ k ][ j ];
    }  
   
   for( int j = 0 ; j < NArcs ; j++ ){
    for( int k = 0 ; k < NComm ; k++ ) {
     if( C[ k ][ j ] < Inf< double >() ) 
      Cmax += C[ k ][ j ];
     if( U[ k ][ j ] > 0 ) 
      Umax += U[ k ][ j ]; 
     sumF += F[ j ]; 
    }
   }

   Umax = 10 * Umax * NNodes * sumQ;
   Cmax = 10 * Cmax * NNodes * sumQ * Umax;
   
  if( ( NCnst != NArcs ) && Active.size() ) {
   weights.resize( NArcs );  // allocate weights for the knapsack sub-problem
   costs.resize( NArcs );  // allocate costs for the knapsack sub-problem
   bound.resize( NArcs );


   for( Index j = 0 ; j < NArcs ; j++ ) {
    if( F.size() == NArcs && sumF>0 ) {
     weights[ j ].resize( NComm + 1 );
     costs[ j ].resize( NComm + 1 );
     }
    else {
     weights[ j ].resize( NComm );
     costs[ j ].resize( NComm );
     }

    for( Index k = 0 ; k < NComm ; k++ ) {
     weights[ j ][ k ] = U[ k ][ j ];
     costs[ j ][ k ] =  C[ k ][ j ] * U[ k ][ j ];
     if( C[ k ][ j ] >= Inf< double >() )
      costs[ j ][ k ] = Cmax; 
     }

    if( F.size() == NArcs && sumF>0) {
     costs[ j ][ NComm ] =  F[ j ];
     weights[ j ][ NComm ] = - UTot[ j ];
     }

    items = NComm;
    if( F.size() == NArcs && sumF>0) {
     items++;
     Integrality.resize( NComm + 1 );
     for( Index k = 0 ; k < NComm ; ++k )
      Integrality[ k ] = false; 
     Integrality[ NComm ] = true; 
     for( Index j = 0 ; j < NArcs ; ++j )
      bound[ j ] = 0;
     }
    else {
     Integrality.resize(NComm);
     for( Index k = 0 ; k < NComm ; ++k )
      Integrality[ k ] = false;
     for( Index j = 0 ; j < NArcs ; ++j )
      bound[ j ] = UTot[ j ];
     }
    }

   v_Block.resize( NArcs );

   for( Index j = 0 ; j < NArcs ; j++ ) {
    auto BKb = new BinaryKnapsackBlock( this );
    BKb->load( items , bound[ j ] , weights[ j ] , costs[ j ] ,
	       Integrality ); 
    BKb->set_objective_sense( false );
    v_Block[ j ] = BKb;
    }
   }
  else {
   weights.resize( NArcs  );  // allocate weights for the knapsack sub-problem
   costs.resize( NArcs  );  // allocate costs for the knapsack sub-problem
 
   for( Index j = 0 ; j < NArcs ; j++ ) {
    if( F.size() == NArcs  && sumF>0) {
     weights[ j ].resize( NComm + 1 );
     costs[ j ].resize( NComm + 1 );
     }
    else {
     weights[ j ].resize( NComm );
     costs[ j ].resize( NComm );
     }

    for( Index k = 0 ;k < NComm ; k++ )
     Cmax += NNodes * C[ k ][ j ];

    for( Index k = 0 ; k < NComm ; k++ ) {
     weights[ j ][ k ] = U[ k ][ j ];
     costs[ j ][ k ] =  C[ k ][ j ] * U[ k ][ j ];
     if( C[ k ][ j ] >= Inf< double >() ) {
      costs[ j ][ k ] = Cmax * U[ k ][ j ];
      weights[ j ][ k ] = Umax;
      }
     }

    if( F.size() == NArcs && sumF>0 ) {
     costs[ j ][ NComm ] = F[ j ];
     weights[ j ][ NComm ] = - UTot[ j ];
     }
    }

   bound.resize( NArcs );
   items = NComm;
   if( F.size() == NArcs && sumF>0 ) {
    items++;
    Integrality.resize( NComm + 1 );
    for( Index j = 0 ; j < NComm ; ++j )
     Integrality[ j ] = false;
    Integrality[ NComm ] = true; 
    for( Index j = 0 ; j < NArcs ; ++j )
     bound[ j ] = 0;
    }
   else {
    Integrality.resize( NComm );
    for( Index j = 0 ; j < NComm ; ++j )
     Integrality[ j ] = false;
    for( Index j = 0 ; j < NArcs ; ++j )
     bound[ j ] = UTot[ j ];
    }

   v_Block.resize( NArcs );
   for( Index j = 0 ; j < NArcs; j++ ) {
    auto BKb = new BinaryKnapsackBlock( this );
    BKb->load( items , bound[ j ] , weights[ j ] , costs[ j ] ,
	       Integrality ); 
    BKb->set_objective_sense( false );
    v_Block[ j ] = BKb;
    }
   }
  }

 // call the base class method to have it done in the sub-Block, if any
 Block::generate_abstract_variables();

 AR |= HasVar;
 }

/*--------------------------------------------------------------------------*/

void MMCFBlock::generate_abstract_constraints( Configuration * stcc )
{
 if( AR & HasMutual )
  return;
  
 // if upper bounds are not there and the Configuration says so, the
 // LB0Constraint are not constructed
 unsigned char sl = 0;
 auto c = dynamic_cast< SimpleConfiguration< int > * >( stcc );
 if( ( ! c ) && f_BlockConfig &&
     f_BlockConfig->f_static_constraints_Configuration )
  c = dynamic_cast< SimpleConfiguration< int > * >(
                        f_BlockConfig->f_static_constraints_Configuration );
 if( c )
  sl = c->value();

 AR = ( AR & ( ~slc ) ) | ( slc * sl );

 // do it in the MCF/BKB respectively
 for( auto blck : v_Block )
  blck->generate_abstract_constraints();

 if( ! ( AR & KnapsackRelaxation ) ) {
  // count number of nonzeroes in each constraint, i.e., #FS( i ) + #BS( i )
  Subset count( get_NArcs() );
  
  // initialize the vectors of coefficients, and reset count[]
  std::vector< LinearFunction::v_coeff_pair > coeffs( get_NArcs() );

  for( Index j = 0 ; j < get_NArcs() ; ++j ) {
   coeffs[ j ].resize( NComm );
   count[ j ] = 0;
   }

  for( Index k = 0 ; k < get_NComm() ; ++k )
   for( Index j = 0 ; j < get_NArcs() ; ++j )
    coeffs[ j ][ k ] = std::make_pair(
      static_cast< MCFBlock * >( v_Block[ k ] )->i2p_x( j ) , double( 1 ) );

  // generate the mutual capacity constraints  - - - - - - - - - - - - - - -
  // each constraint is an inequality, i.e., RHS = UTot[ j ]
  if( ( NCnst != NArcs ) && Active.size() ) {
   MCs.resize( NCnst );
   for( Index j = 0 ; j < NCnst ; ++j ) {
    if( UTot[ Active[ j ] ] >= Inf< double >() )
     throw( std::logic_error( "Constraint required to have a finite rhs" ) );

    MCs[ j ].set_function( new LinearFunction(
				  std::move( coeffs[ Active[ j ] ] ) , 0 ) );
    MCs[ j ].set_rhs( UTot[ Active[ j ] ] );
    MCs[ j ].set_lhs( -Inf< double >() );
    }
   }
  else{
   MCs.resize( get_NArcs() );
   for( Index j = 0 ; j < get_NArcs() ; ++j ) {
    if( UTot[ j ] >= Inf< double >() )
     throw( std::logic_error( "Constraint required to have a finite rhs" ) );
    MCs[ j ].set_rhs( UTot[ j ] );
    MCs[ j ].set_lhs( -Inf< double >() );
    MCs[ j ].set_function( new LinearFunction( std::move( coeffs[ j ] ) , 0 ) );
    }
   }

  add_static_constraint( MCs , "Mut" );
  }
 else { 
  // count number of nonzeroes in each constraint, i.e., #FS( i ) + #BS( i )
  std::vector< Subset > count( get_NComm() );
 
  // initialize the vectors of coefficients, and reset count[]
  boost::multi_array< LinearFunction::v_coeff_pair , 2 > coeffs(
			    boost::extents[ get_NComm()] [ get_NNodes() ] );

  for( Index k = 0 ; k < get_NComm() ; ++k ) {
   count[ k ].resize( get_NNodes() );
   if( NCnst != NArcs && Active.size() ) {
    for( Index i = 0 ; i < NArcs ; ++i ) {
     count[ k ][ Startn[ i ] - 1 ]++;
     count[ k ][ Endn[ i ] - 1 ]++;
     }
    }
   else {
    for( Index i = 0 ; i < get_NArcs() ; ++i ) {
     count[ k ][ Startn[ i ] - 1 ]++;
     count[ k ][ Endn[ i ] - 1 ]++;
     }
    } 
   }
  
  for( Index k = 0 ; k < get_NComm() ; ++k )
   for( Index i = 0 ; i < get_NNodes() ; ++i ) {
    coeffs[ k ][ i ].resize( count[ k ][ i ] );
    count[ k ][ i ] = 0;
    }

  // construct the vector of coefficients, static phase
  for( Index k = 0 ; k < get_NComm() ; ++k ) {
   for( Index i = 0; i < get_NArcs() ; ++i ) {
    if( Startn[ i ] == Endn[ i ])
     continue;
    coeffs[ k ][ Startn[ i ] - 1 ][ count[ k ][ Startn[ i ] - 1 ]++ ] =
     std::make_pair( ( static_cast< BinaryKnapsackBlock * >( v_Block[ i ] )->get_Var( k )) , double( U[ k ][ i ] ) );
    coeffs[ k ][ Endn[ i ] - 1 ][ count[ k ][ Endn[ i ] - 1 ]++ ] =
     std::make_pair( ( static_cast< BinaryKnapsackBlock * >( v_Block[ i ] )->get_Var( k )) , double( - U[ k ][ i ]  )  ) ;
    }
   }

  FCs.resize( boost::extents[ get_NComm() ][ get_NNodes() ] );
  for( Index i = 0; i < get_NNodes() ; ++i ) {
   for( Index k = 0 ; k < get_NComm() ; ++k ) {  
    (FCs)[ k ][ i ].set_both( B.empty() ? 0 : B[ k ][ i ] );
    (FCs)[ k ][ i ].set_function(
		  new LinearFunction( std::move( coeffs[ k ][ i ] ) , 0 ) );
    }
   }

  add_static_constraint( FCs , "Flow" );

  if( AR & slc ) {
   boost::multi_array< LinearFunction::v_coeff_pair , 2 > coeffsSLC(
			     boost::extents[ get_NComm() ][ get_NArcs() ] );

   for( Index k = 0 ; k < get_NComm() ; ++k )
    for( Index i = 0 ; i < get_NArcs() ; ++i )
     coeffsSLC[ k ][ i ].resize( 2 );

   // construct the vector of coefficients, static phase
   for( Index k = 0 ; k < get_NComm() ; ++k ) {
    for(  Index i = 0; i < get_NArcs() ; ++i ) {
     coeffsSLC[ k ][ i ][ 0 ] =
      std::make_pair( ( static_cast< BinaryKnapsackBlock * >( v_Block[ i ] )->get_Var( k )) , double( 1 ) );
     coeffsSLC[ k ][ i ][ 1 ] =
      std::make_pair( ( static_cast< BinaryKnapsackBlock * >( v_Block[ i ] )->get_Var( get_NComm() )) , double( -1 )  );
     }
    }

   SLCs.resize( boost::extents[ get_NComm() ][ get_NArcs() ] );
 
   for( Index i = 0; i < get_NArcs() ; ++i ) {
    for( Index k = 0 ; k < get_NComm() ; ++k ) {   
     (SLCs)[ k ][ i ].set_lhs( -Inf< double >() );
     (SLCs)[ k ][ i ].set_rhs( 0 );
     (SLCs)[ k ][ i ].set_function(
	      new LinearFunction( std::move( coeffsSLC[ k ][ i ] ) , 0 ) );
     }
    }

   add_static_constraint( SLCs , "StrongForcCons" );
   }
  }

 AR |= HasMutual;

 }  // end( MMCFBlock::generate_abstract_constraints() )

/*--------------------------------------------------------------------------*/
/*-------------------------- PROTECTED METHODS -----------------------------*/
/*--------------------------------------------------------------------------*/
 
void MMCFBlock::PreProcess( FNumber IncUk , FNumber DecUk ,
			    FNumber IncUjk , FNumber DecUjk ,
			    FNumber ChgDfct , CNumber DecCsts )
{
 if( ChgDfct >= Inf< double >() )
  throw( std::invalid_argument( "infinite ChgDfct" ) );
 if( DecCsts > Inf< double >() )
  throw( std::invalid_argument( "infinite DecCsts" ) );

 // allocate (temporary) data structures- - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 Active.resize( NArcs );

 Vec_FNumber tmpv( NComm );
 Subset srck( NComm );

 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // squeeze rhss, declare arcs as "non-existent", etc.- - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 // ensure that all arcs entering/leaving a non-existent node do not exist- -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 for( Index k = 0 ; k < NComm ; k++ )
  for( Index i = 0 ; i < NArcs ; i++ )
   if( ( B[ k ][ Startn[ i ] - StrtNme ] == Inf< double >() ) ||
       ( B[ k ][ Endn[ i ] - StrtNme ] == Inf< double >() ) )
    C[ k ][ i ] = Inf< double >();

 // ensure that all non-existent arcs have zero capacity- - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 for( Index k = 0 ; k < NComm ; k++ )
  for( Index i = 0 ; i < NArcs ; i++ )
   if( C[ k ][ i ] == Inf< double >() )
    U[ k ][ i ] = 0;

 // a *very* rough estimate of the max. flow across any arc is computed for
 // each commodity, and it is stored in tmpv[ k ] - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 FNumber maxU = 0;
 for( Index k = NComm ; k-- ; ) {
  // first, count the flow out of the sources

  Index srcs = 0;    // meanwhile, the sources are counted
  FNumber maxUk = 0;
  for( auto &Bk : B[k] )
   if( Bk < 0 ) {
    srcs++;
    maxUk -= Bk;
    }

  // now the contribution of arcs with potentially negative costs
  for( Index j = 0 ; j < NArcs ; j++ ) {
   const FNumber tMF = std::min( U[ k ][ j ] , UTot[ j ] );

   if( C[ k ][ j ] < DecCsts ) {
    if( tMF >= Inf< double >() )
     throw( std::invalid_argument( "negative cost, infinite capacity" ) );
    maxUk += tMF;
    }
   }

  srck[ k ] = srcs;
  maxUk += ( ( NNodes + 1 ) / 2 ) * ChgDfct;  // count potential changes in
  maxU += ( tmpv[ k ] = maxUk );              // the deficits
  }

 // detection of redundant mutual capacity constraints is attempted, and- - -
 // all the mutual capacity upper bounds are turned to finite values- - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 for( Index i = NCnst = 0 ; i < NArcs ; i++ ) {
  if( ( ! IncUk ) && ( ! UTot[ i ] ) ) {   // if mutual capacities can not
   for( Index k = NComm ; k-- ; ) {        // increase, and UTot[] == 0 ...
    C[ k ][ i ] = Inf< double >();         // ... this arc does not exist
    U[ k ][ i ] = 0;
    }

   continue;
   }

  if( DecUk == Inf< double >() ) {     // all mutual capacity constraints exist
   if( UTot[ i ] == Inf< double >() )  // but those that are declared non-so
    UTot[ i ] = maxU;                  // ensure that UTot is "finite" anyway
   else
    Active[ NCnst++ ] = i;

   continue;
   }

  // compute is an upper bound on the max quantity of flow (of any commodity)
  // on arc i: if capacities can increase indefinitely, the only bound is
  // the total quantity of flow in the graph

  FNumber Ui = 0;
  if( IncUjk < Inf< double >() )
   for( Index k = NComm ; k-- ; )
    if( U[ k ][ i ] == Inf< double >() )
     Ui += tmpv[ k ];
    else
     Ui += std::min( tmpv[ k ] , U[ k ][ i ] + IncUjk );
  else
   Ui = maxU;

  // note: when e.g. the mutual capacity and the sum of all the individual
  // capacities of an arc are identical, the arc is marked as "inactive"; this
  // is an arbitrary choice, since one could as well keep it and eliminate all
  // the individual capacities

  if( UTot[ i ] >= Ui - DecUk )
   UTot[ i ] = Ui;
  else
   Active[ NCnst++ ] = i;

  }  // end for( i )

 if( NCnst < NArcs )
  Active[ NCnst ] = Inf< Index >();

 // now a squeeze of single-commodity capacities is attempted, and SPTs are -
 // definitively recognized - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // meanwhile, construct the "active" individual capacity constraints

 for( Index k = 0 ; k < NComm ; k++ ) {
  ActiveK[ k ].clear();
  ActiveK[ k ].resize( NArcs );

  Index cnt = 0;  // active individual capacity constraints
  Index count1 = 0;
  Index count2 = 0;
  for( Index i = 0 ; i < NArcs ; i++ ) {
   bool Ai = ( Active[count1] == i );    // true if arc i is "active"
   if( Ai )
    count1++;

   if( C[ k ][ i ] == Inf< double >() )  // a non-existent arc
    continue;

   if( ( ! IncUjk ) && ( ! U[ k ][ i ] ) ) {
    // an arc that can be declared non-existent by its capacity
    // (that will never increase)
    C[ k ][ i ] = Inf< double >();
    continue;
    }

   if( DecUjk < Inf< double >() ) {
    // if individual capacities cannot decrease forever, then the
    // individual capacity constraint of some existing arc can be
    // declared redundant

    if( U[ k ][ i ] >= tmpv[ k ] + DecUjk ) {
     // the constraint is redundant because there will never be that much
     // flow in the graph: anyway, give it a "nice" finite value
     U[ k ][ i ] = std::min( tmpv[ k ] , UTot[ i ] );
     continue;
     }

    if( ( IncUk < Inf< double >() ) &&
	( Ai && ( U[ k ][ i ] >= UTot[ i ] + IncUk + DecUjk ) ) ) {
     // if mutual capacities cannot increase forever, some individual
     // capacities may be declared redundant by the mutual capacity
     // note that the mutual capacity of an arc can be used to declare
     // that the individual capacity is redundant only if the arc is
     // "active", as "inactive" arcs precisely mean that no mutual
     // capacity constraint is imposed on them (i.e., the value of
     // UTot[ i ] is not really meaningful and can be ignored)

     U[ k ][ i ] = UTot[ i ];  // give it a "nice" finite value anyway
     continue;
     }
    }

   ActiveK[ k ][ count2++ ] = i;
   cnt++;

   }  // end for( i )

  ///if( ( ! cnt ) && ( srck[ k ] == 1 ) )
  /// PT[ k ] = kSPT;

  NamesK[ k + 1 ] = NamesK[ k ] + cnt;

  if( cnt >= NArcs )   // all individual capacity constraints are active
   ActiveK[ k ].clear();
  else { // some are active, some are not
   ActiveK[ k ].resize(cnt + 1);
   ActiveK[ k ][ cnt ] = Inf< Index >();
   }
  }   // end for( k )

 if( NCnst >= NArcs )
  Active.clear();

 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // find and eliminate redundancies in the data structures- - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 // examine B[] - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 BIsCpy.resize( NComm , bool( false ) );

 bool cpy = false;
 for( Index k = 1 ; k < NComm ; k++ )
  for( Index i = 0 ; i < k ; i++ )
   if( ( ! BIsCpy[ i ] ) && ( B[ k ] == B[ i ] ) ) {
    BIsCpy[ k ] = cpy = true;
    B[ k ].resize( B[ i ].size() );
    std::copy( B[ i ].begin() , B[ i ].end(), B[ k ].begin() );
    break;
    }

 if( ! cpy )
  BIsCpy.clear();

 // examine U[] and UTot- - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 UIsCpy.resize( NComm , bool( false ) );

 cpy = false;
 if( U[ 0 ] == UTot ) {
  UIsCpy[ 0 ] = cpy = true;
  U[ 0 ].resize( UTot.size() );
  std::copy( UTot.begin() , UTot.end(), U[ 0 ].begin() );
  }

 for( Index k = 1 ; k < NComm ; k++ )
  for( Index i = 0 ; i < k ; i++ )
   if( ( ! UIsCpy[ i ] ) && ( U[ k ] == U[ i ] ) ) {
    UIsCpy[ k ] = cpy = true;
    U[ k ].resize( U[ i ].size() );
    std::copy( U[ i ].begin() , U[ i ].end(), U[ k ].begin() );
    break;
    }

 if( ! cpy )
  UIsCpy.clear();

 // examine C[] - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 CIsCpy.resize( NComm , bool( false ) );

 cpy = false;
 for( Index k = 1 ; k < NComm ; k++ )
  for( Index i = 0 ; i < k ; i++ )
   if( ( ! CIsCpy[ i ] ) && ( C[ k ] == C[ i ] ) ) {
    CIsCpy[ k ] = cpy = true;
    C[ k ].resize( C[ i ].size() );
    std::copy( C[ i ].begin() , C[ i ].end(), C[ k ].begin() );
    break;
    }

 if( ! cpy )
  CIsCpy.clear();

 // cleanup - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 srck.clear();
 tmpv.clear();

 }  // end( MMCFBlock::PreProcess )

/*--------------------------------------------------------------------------*/

void MMCFBlock::print( std::ostream & output , char vlvl ) const
{
 output << "MMCFBlock with " << get_NComm() << " commodities, "
	<< get_NNodes() << " nodes and " << get_NArcs() << " arcs"
	<< std::endl;
 }

/*--------------------------------------------------------------------------*/

void MMCFBlock::serialize( netCDF::NcGroup & group ) const
{
 // call the method of Block- - - - - - - - - - - - - - - - - - - - - - - - -

 Block::serialize( group );

 // now the MMCFBlock data- - - - - - - - - - - - - - - - - - - - - - - - - -
 netCDF::NcDim nn = group.addDim( "NNodes" , get_NNodes() );
 netCDF::NcDim na = group.addDim( "NArcs" , get_NArcs() );
 netCDF::NcDim nc = group.addDim( "NComm" , get_NComm() );
 netCDF::NcDim ncnst = group.addDim( "NCnst" , NCnst);

 ( group.addVar( "SN" , netCDF::NcUint64() , na ) ).putVar( Startn.data() );

 ( group.addVar( "EN" , netCDF::NcUint64() , na ) ).putVar( Endn.data() );

 ( group.addVar( "Utot" , netCDF::NcDouble() , na ) ).putVar( UTot.data() );

 if( F.size() == NArcs )
  ( group.addVar( "F" , netCDF::NcDouble() , na ) ).putVar( F.data() );

 ::serialize( group, "U", netCDF::NcDouble(), U, {nc,na});
              
 ::serialize( group, "B", netCDF::NcDouble(), B, {nc,nn});
              
 ::serialize( group, "C", netCDF::NcDouble(), C, {nc,na});

 }  // end( MCFBlock::serialize )

/*--------------------------------------------------------------------------*/

void MMCFBlock::deserialize( const netCDF::NcGroup & group )
{
 // erase previous instance, if any- - - - - - - - - - - - - - - - - - - - - -

 if( NNodes || NComm || get_NArcs() )
   MMCFBlock();

 // read problem data- - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 auto nn = group.getDim( "NNodes" );
 if( nn.isNull() )
  throw( std::logic_error( "NNodes dimension is required" ) );
 NNodes = nn.getSize();

 auto na = group.getDim( "NArcs" );
 if( na.isNull() )
  throw( std::logic_error( "NArcs dimension is required" ) );
 NArcs = na.getSize();
 
 auto nc = group.getDim( "NComm" );
 if( nc.isNull() )
  throw( std::logic_error( "NComm dimension is required" ) );
 NComm = nc.getSize();
 
 Index NCnst = NArcs;
 auto ncnst = group.getDim( "NCnst" );
 if( nc.isNull() )
  throw( std::logic_error( "NCnst dimension is required" ) );
 NCnst = ncnst.getSize();
 
 auto sn = group.getVar( "SN" );
 if( sn.isNull() )
  throw( std::logic_error( "Starting Nodes not found" ) );

 Startn.resize( NArcs );
 sn.getVar( Startn.data() );

 auto en = group.getVar( "EN" );
 if( en.isNull() )
  throw( std::logic_error( "Ending Nodes not found" ) );

 Endn.resize( NArcs );
 en.getVar( Endn.data() );
 
 auto ut = group.getVar( "Utot" );
 if( ut.isNull() )
  throw( std::logic_error( "Total capacities not found" ) );

 UTot.resize( NArcs );
 ut.getVar( UTot.data() );

 auto fc = group.getVar( "F" );
 if( ! fc.isNull() ){
  F.resize( NArcs );
  fc.getVar( F.data() );
  }

 U.resize( NComm );
 for( int i = 0 ; i< NComm ; i++ )
  U[ i ].resize( NArcs );
 
 B.resize( NComm );
 for( int i = 0 ; i< NComm ; i++ )
  B[ i ].resize(NNodes); 
 
 C.resize( NComm );
 for( int i = 0 ; i< NComm ; i++)
  C[ i ].resize( NArcs ); 
 
 ::deserialize( group , "U" , U );
 ::deserialize( group , "B" , B );
 ::deserialize( group , "C" , C ); 
 
 // common initializations- - - - - - - - - - - - - - - - - - - - - - - - - -
 CmnIntlz();

 // call the method of Block- - - - - - - - - - - - - - - - - - - - - - - - -
 // inside this the NBModification, the "nuclear option",  is issued

 Block::deserialize( group );

 } // end( MMCFBlock::deserialize )

/*--------------------------------------------------------------------------*/

void MMCFBlock::CmnIntlz( void )
{
 // some initializations that are common to all the constructors- - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 StrtNme = 1;
 DrctdPrb = true;

 PT.resize( NComm );

 Index k = NComm;
 for( ; k-- ; )
  PT[ k ] = kMCF;

 // find arcs that might have individual capacity constraints - - - - - - - -
 // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

 ActiveK.resize( NComm );
 NamesK.resize( NComm + 1 );

 for( NamesK[ 0 ] = NCnst , k = 0 ; k < NComm ; k++ ) {
  // first, count how many active constraints there are - - - - - - - - - - -

  Index i = 0;
  Index cnt = 0;
  // cCRow Ck = C[ k ];
  // cFRow Uk = U[ k ];

  for( ; i < NArcs ; i++ )
   if( ( C[ k ][ i ] < Inf< CNumber >() ) &&
       ( U[ k ][ i ] < Inf< FNumber >() ) )
    cnt++;

  // second, (if necessary) construct the actual vector of indices - - - - - -

  NamesK[ k + 1 ] = NamesK[ k ] + cnt;

  if( cnt < NArcs ) {
   ActiveK[ k ].resize( cnt + 1 );
   for( i = 0 ; i < NArcs ; i++ )
    if( ( C[ k ][ i ] < Inf< CNumber >() ) &&
	( U[ k ][ i ] < Inf< FNumber >() ) )
     ActiveK[ k ].push_back( i );

   ActiveK[ k ].push_back( Inf< Index >() );
   }
  else
   ActiveK[ k ].clear();

  }  // end( for( k ) )
 }  // end( CmnIntlz )

/*--------------------------------------------------------------------------*/
/*-------------------------- PRIVATE METHODS -------------------------------*/
/*--------------------------------------------------------------------------*/

void MMCFBlock::guts_of_destructor( void )
{
 /* clear() all Constraint to ensure that they do not bother to un-register
    themselves from Variable that are going to be deleted anyway. Then
    deletes all the "abstract representation", if any. */

 for( auto & cnst : MCs )
  cnst.clear();
 {
  const auto sup = FCs.data() + FCs.num_elements();
  for( auto it = FCs.data() ; it != sup ; ++it )
   it->clear();
  }
 {
  const auto sup = SLCs.data() + SLCs.num_elements();
  for( auto it = SLCs.data() ; it != sup ; ++it )
   it->clear();
  }

 MCs.clear();
 FCs.resize( boost::extents[ 0 ][ 0 ] );
 SLCs.resize( boost::extents[ 0 ][ 0 ] );

 for( auto bk : v_Block )
  delete bk;

 v_Block.clear();

 NXtrV = NXtrC = 0;
 IdxBeg.clear();
 CoefIdx.clear();
 CoefVal.clear();
 C.clear();
 U.clear();
 B.clear();
 I.clear();

 UTot.clear();
 F.clear();

 Startn.clear();
 Endn.clear();

 NamesK.clear();
 Active.clear();
 ActiveK.clear();
 PT.clear();

 CIsCpy.clear();
 UIsCpy.clear();
 BIsCpy.clear();

 // explicitly reset all Constraint and Variable
 // this is done for the case where this method is called prior to re-loading
 // a new instance: if not, the new representation would be added to the
 // (no longer current) one
 reset_static_constraints();
 // not needed, there isn't any - reset_static_variables();
 // not needed, there isn't any - reset_dynamic_constraints();
 // not needed, there isn't any - reset_dynamic_variables();
 // not needed, there isn't any - reset_objective();

 AR = 0;
 
 }  // end( guts_of_destructor )

/*--------------------------------------------------------------------------*/
/*---------------------- End File MMCFBlock.cpp ----------------------------*/
/*--------------------------------------------------------------------------*/

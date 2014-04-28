#include <iostream>

#include <boost/shared_ptr.hpp>
#include <gdal/ogrsf_frmts.h>
#include <geos_c.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_2_algorithms.h>
#include <CGAL/create_straight_skeleton_2.h>

typedef CGAL::Exact_predicates_inexact_constructions_kernel K ;

typedef K::Point_2                    Point ;
typedef CGAL::Polygon_2<K>            Polygon ;
typedef CGAL::Straight_skeleton_2<K>  Ss ;

typedef boost::shared_ptr<Ss> SsPtr ;

inline
void IsValid(void* ptr, const std::string message)
{
  if(ptr == NULL){
    std::cerr << message << std::endl;
    exit(1);
  }
}

Polygon GeomToPoly(OGRGeometry* geometry){
  OGRLinearRing *ring = dynamic_cast<OGRLinearRing *>(dynamic_cast<OGRPolygon *>(geometry)->getExteriorRing());
  Polygon skeleton;
  for(int i=0; i < ring->getNumPoints() - 1; i++)
    if(CGAL::is_finite(ring->getX(i)) && CGAL::is_finite(ring->getY(i)))
      skeleton.push_back(Point(ring->getX(i), ring->getY(i)));
  return skeleton;
}


OGRGeometry* BuildMultiLine(OGRGeometry* geometry){
  double simplify = 0.01;
  OGRPolygon* poly = dynamic_cast<OGRPolygon *>(geometry->Simplify(simplify));
  while(poly == NULL){
    simplify -= 0.001;
    poly = dynamic_cast<OGRPolygon *>(geometry->Simplify(simplify));
  }

  Polygon skeleton = GeomToPoly(poly);
  if(skeleton.size() < 3) return NULL;

  if(skeleton.is_counterclockwise_oriented() == 0) { skeleton.reverse_orientation(); }
  std::cout << "\nSkeletonizing." << std::endl;
  SsPtr iss = CGAL::create_interior_straight_skeleton_2(skeleton);
  if(!iss.get()) IsValid(NULL, "No skeleton!");
  std::cout << "Computed Skeleton." << std::endl;
  OGRLineString* line = NULL;
  // And finally append points to our shapefile
  double edge = 0;
  Ss::Halfedge_iterator vi = iss->halfedges_begin();
  for(; vi != iss->halfedges_end(); ++vi){
    OGRPoint point  = OGRPoint(vi->vertex()->point().x(), vi->vertex()->point().y());
    OGRPoint npoint = OGRPoint(vi->next()->vertex()->point().x(), vi->next()->vertex()->point().y());
    OGRLineString segment = OGRLineString();
    segment.addPoint(&point);
    segment.addPoint(&npoint);
    if(line == NULL) { line = new OGRLineString; }
    OGRLineString *tmp;
    ++edge;
    if(vi->vertex()->is_skeleton() && vi->next()->vertex()->is_skeleton() && segment.Within(geometry)) {
      tmp = reinterpret_cast<OGRLineString *>(line->Union(&segment));
      if(tmp != NULL) {
        std::cout << "\r" <<  (int) (edge / (double)iss->size_of_halfedges() * 100.0) << "% ";
        std::cout.flush();
        delete line;
        line = tmp;
      }
    }
  }
  OGRGeometryFactory::destroyGeometry(poly);
  return line;
}


int main(int argc, char **argv)
{
  // Get data from ogr
  OGRRegisterAll();
  std::cout << "Opening: " << argv[1] << std::endl;

  OGRDataSource *shp = OGRSFDriverRegistrar::Open(argv[1], FALSE);
  IsValid(shp, "Error opening file.");

  std::cout << "Shape contains " << shp->GetLayerCount() << " layers." << std::endl;
  OGRLayer *layer = shp->GetLayerByName(argv[2]);
  IsValid(layer, "Couldn't grab layer");

  OGRSpatialReference *srcSRS = NULL;
  srcSRS = layer->GetSpatialRef();

  // Set up writing
  const char *kDriverName = "ESRI Shapefile";
  OGRSFDriver *shpDriver = OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName(kDriverName);
  IsValid(shpDriver, "Couldn't grab the shapefile driver.");

  IsValid(argv[3], "Please provide a output shp.");
  std::cout << "Writing to: " << argv[3] << std::endl;
  OGRDataSource *shpOut = shpDriver->CreateDataSource(argv[3], NULL);
  IsValid(shpOut, "Couldn't open output file");

  OGRLayer *outLayer = shpOut->CreateLayer(layer->GetName(), srcSRS, wkbMultiLineString, NULL);
  IsValid(outLayer, "Couldn't create an output layer");

  // copy over the fields from the source file
  OGRFeatureDefn *source = layer->GetLayerDefn();
  for(int i=0; i < source->GetFieldCount(); i++){
    OGRFieldDefn *field = source->GetFieldDefn(i);
    if(outLayer->CreateField(field) != OGRERR_NONE) {
      std::cout << "Couldn't make layer" << std::endl; exit(1);
    };
  }

  // Loop through features and grab the hull and put it into CGAL then
  // skeletonize the points
  OGRFeature *feature;
  int count = 0;
  while((feature = layer->GetNextFeature()) != NULL)
  {
    OGRMultiPolygon *geometry = dynamic_cast<OGRMultiPolygon *>(OGRGeometryFactory::forceToMultiPolygon(feature->GetGeometryRef()));
    IsValid(geometry, "No geometry.");

    OGRFeature *outFeature = OGRFeature::CreateFeature(outLayer->GetLayerDefn());
    IsValid(outFeature, "Couldn't make a feature.");

    for(int i=0; i < source->GetFieldCount(); i++){
      OGRField *field = feature->GetRawFieldRef(i);
      outFeature->SetField(i, field);
    }

    OGRGeometry* line = NULL;
    for(int i=0; i < geometry->getNumGeometries(); i++){
      OGRGeometry* segment = BuildMultiLine(geometry->getGeometryRef(i));
      if(segment != NULL){
        if(line == NULL) { line = new OGRLineString; }
        OGRGeometry* tmp = line->Union(segment);
        if(tmp != NULL){
          delete line;
          line = tmp;
        }
        delete segment;
      }
    }
    outFeature->SetGeometry(line);
    if(outLayer->CreateFeature(outFeature) != OGRERR_NONE){
      std::cout << "Couldn't create feature." << std::endl;
      exit(1);
    }

    // clean up
    OGRFeature::DestroyFeature(outFeature);
    std::cout << std::endl << ++count << std::endl;
  }

  // cleanup
  OGRDataSource::DestroyDataSource(shp);
  OGRDataSource::DestroyDataSource(shpOut);
  return 0;
}

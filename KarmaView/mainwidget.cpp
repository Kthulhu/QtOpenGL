#include "mainwidget.h"

#include <cmath>

#include <OpenGLBuffer>
#include <OpenGLFunctions>
#include <OpenGLInstanceGroup>
#include <OpenGLShaderProgram>
#include <OpenGLUniformBufferObject>
#include <OpenGLVertexArrayObject>
#include <OpenGLFramebufferObject>
#include <OpenGLRenderbufferObject>
#include <OpenGLTexture>
#include <OpenGLInstance>
#include <OpenGLMaterial>
#include <OpenGLPointLightGroup>
#include <OpenGLSpotLightGroup>
#include <OpenGLDirectionLightGroup>
#include <OpenGLRenderBlock>
#include <OpenGLPointLight>
#include <OpenGLSpotLight>
#include <OpenGLDirectionLight>
#include <OpenGLRenderer>

#include <KCamera3D>
#include <OpenGLDebugDraw>
#include <KInputManager>
#include <KMatrix4x4>
#include <KPanGesture>
#include <KPinchGesture>
#include <KPointF>
#include <KTransform3D>
#include <KVector2D>
#include <KVertex>
#include <KStaticGeometry>

// Bounding Volumes
#include <KAabbBoundingVolume>
#include <KSphereBoundingVolume>
#include <KEllipsoidBoundingVolume>
#include <KOrientedBoundingVolume>

#include <Qt>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QOpenGLFramebufferObject>
#include <OpenGLMesh>
#include <KHalfEdgeMesh>
#include <KLinq>
#include <OpenGLUniformManager>
#include <QMainWindow>
#include <QApplication>

#include <GBufferPass>
#include <LightPass>

enum PresentType
{
  PresentComposition,
  PresentDepth,
  PresentLinearDepth,
  PresentPosition,
  PresentViewNormal,
  PresentDiffuse,
  PresentSpecular,
  PresentVelocity,
  PresentLightAccumulation,
  MaxPresentations
};

/*******************************************************************************
 * MainWidgetPrivate
 ******************************************************************************/
class MainWidgetPrivate : protected OpenGLFunctions, public OpenGLRenderer
{
public:
  MainWidgetPrivate(MainWidget *parent);

  // GL Methods
  void initializeGL();
  void resizeGL(int width, int height);
  void paintGL();

  // GL Paint Steps
  void commitGL();
  void renderGL();
  void buildGBuffer();
  void renderLights();
  void composeScene();

  void loadObj(const QString &fileName);
  void openObj();
  void drawBoundaries();
  void drawBackbuffer();
  OpenGLRenderBlock &currentRenderBlock();
  OpenGLRenderBlock &previousRenderBlock();
  void swapRenderBlocks();
  void fixRenderBlocks();
  void updateRenderBlocks();
  void renderGeometry();

  // Scene Data
  KCamera3D m_camera;
  OpenGLRenderBlock m_renderBlocks[2];
  int m_renderBlockIndex[2];
  LightPass *m_lightPass;

  // OpenGL State Information
  bool m_paused;
  OpenGLMesh m_openGLMesh;
  KHalfEdgeMesh *m_halfEdgeMesh;
  KHalfEdgeMesh *m_quad;
  KHalfEdgeMesh *m_floor;
  OpenGLMesh m_quadGL;
  OpenGLMesh m_floorGL;
  typedef std::tuple<KVector3D,KVector3D> QueryResultType;
  std::vector<QueryResultType> m_boundaries;
  OpenGLShaderProgram *m_textureDrawer;
  OpenGLInstanceGroup m_instanceGroup;
  OpenGLInstanceGroup m_floorGroup;
  OpenGLInstance *m_floorInstance;
  PresentType m_presentation;
  OpenGLShaderProgram *m_deferredPrograms[MaxPresentations];

  // Bounding Volumes
  KAabbBoundingVolume *m_aabbBV;
  KSphereBoundingVolume *m_sphereCentroidBV;
  KSphereBoundingVolume *m_sphereRittersBV;
  KSphereBoundingVolume *m_sphereLarssonsBV;
  KSphereBoundingVolume *m_spherePcaBV;
  KEllipsoidBoundingVolume *m_ellipsoidPcaBV;
  KOrientedBoundingVolume *m_orientedPcaBV;
  KStaticGeometry *m_staticGeometryBottomUp7;
  KStaticGeometry *m_staticGeometryBottomUp500;
  KStaticGeometry *m_staticGeometryTopDown7;
  KStaticGeometry *m_staticGeometryTopDown500;
  KStaticGeometry *m_staticGeometry;

  std::vector<OpenGLInstance*> m_instances;
  std::vector<OpenGLRenderPass*> m_passes;
  float m_ambientColor[4];
  float m_atmosphericColor[4];

  // Touch Information
  float m_dragVelocity;
  KVector3D m_dragAxis;

  // Runtime
  bool b_rX, b_rY, b_rZ;
  bool b_bv[8];
  int m_minDraw, m_maxDraw;

  // Parent
  MainWidget *m_parent;
};

MainWidgetPrivate::MainWidgetPrivate(MainWidget *parent) :
  m_halfEdgeMesh(Q_NULLPTR), m_parent(parent),
  m_presentation(PresentComposition), m_paused(false), m_staticGeometry(0), m_minDraw(0), m_maxDraw(std::numeric_limits<size_t>::max())
{
  m_ambientColor[0] = m_ambientColor[1] = m_ambientColor[2] = 0.2f;
  m_ambientColor[3] = 1.0f;
  m_atmosphericColor[0] = m_atmosphericColor[1] = m_atmosphericColor[2] = 0.0f;
  m_atmosphericColor[3] = 1.0f;
  m_camera.setTranslation(0.0f, 3.0f, 10.0f);
  m_camera.setRotation(-20.0f, 1.0f, 0.0f, 0.0f);
  b_rX = b_rY = b_rZ = false;
  for (int i = 0; i < 8; ++i)
    b_bv[i] = false;
  m_staticGeometryBottomUp7 = m_staticGeometryBottomUp500 = m_staticGeometryTopDown7 = m_staticGeometryTopDown500 = 0;
  m_renderBlockIndex[0] = 0; // Current Index
  m_renderBlockIndex[1] = 1; // Previous Index

  m_lightPass = new LightPass;
  m_passes.push_back(new GBufferPass);
  m_passes.push_back(m_lightPass);
}

void MainWidgetPrivate::initializeGL()
{
  GL::setInstance(this);
  initializeOpenGLFunctions();

  // Set Uniform Buffers
  OpenGLUniformManager::setUniformBufferIndex("CurrentRenderBlock"  , 1);
  OpenGLUniformManager::setUniformBufferIndex("PreviousRenderBlock" , 2);
  OpenGLUniformManager::setUniformBufferIndex("SpotLightProperties" , 3);

  // Set Texture Samplers
  OpenGLUniformManager::setTextureSampler("depthTexture"      , OpenGLTexture::numTextureUnits() - 1);
  OpenGLUniformManager::setTextureSampler("geometryTexture"   , OpenGLTexture::numTextureUnits() - 2);
  OpenGLUniformManager::setTextureSampler("materialTexture"   , OpenGLTexture::numTextureUnits() - 3);
  OpenGLUniformManager::setTextureSampler("surfaceTexture"    , OpenGLTexture::numTextureUnits() - 4);
  OpenGLUniformManager::setTextureSampler("lightbufferTexture", OpenGLTexture::numTextureUnits() - 5);

  for (OpenGLRenderPass *pass : m_passes)
  {
    pass->initialize();
  }
}

void MainWidgetPrivate::loadObj(const QString &fileName)
{
  // Remove old mesh
  bool oldValue = m_paused;
  m_paused = true;
  delete m_halfEdgeMesh;
  m_boundaries.clear();

  // Initialize an object
  quint64 ms;
  QElapsedTimer timer;
  {
    {
      timer.start();
      m_halfEdgeMesh = new KHalfEdgeMesh(m_parent, fileName);
      ms = timer.elapsed();
      qDebug() << "Create HalfEdgeMesh (sec)    :" << float(ms) / 1e3f;
    }
    {
      timer.start();
      m_halfEdgeMesh->calculateVertexNormals();
      ms = timer.elapsed();
      qDebug() << "Calculate Normals (sec)      :" << float(ms) / 1e3f;
    }
    {
      timer.start();
      m_aabbBV = new KAabbBoundingVolume(*m_halfEdgeMesh, KAabbBoundingVolume::MinMaxMethod);
      m_sphereCentroidBV = new KSphereBoundingVolume(*m_halfEdgeMesh, KSphereBoundingVolume::CentroidMethod);
      m_sphereRittersBV = new KSphereBoundingVolume(*m_halfEdgeMesh, KSphereBoundingVolume::RittersMethod);
      m_sphereLarssonsBV = new KSphereBoundingVolume(*m_halfEdgeMesh, KSphereBoundingVolume::LarssonsMethod);
      m_spherePcaBV = new KSphereBoundingVolume(*m_halfEdgeMesh, KSphereBoundingVolume::PcaMethod);
      m_ellipsoidPcaBV = new KEllipsoidBoundingVolume(*m_halfEdgeMesh, KEllipsoidBoundingVolume::PcaMethod);
      m_orientedPcaBV = new KOrientedBoundingVolume(*m_halfEdgeMesh, KOrientedBoundingVolume::PcaMethod);
      ms = timer.elapsed();
      qDebug() << "Create Bounding Volumes (sec):" << float(ms) / 1e3f;
    }
    {
      m_parent->makeCurrent();
      timer.start();
      m_openGLMesh.create(*m_halfEdgeMesh);
      m_instanceGroup.setMesh(m_openGLMesh);
      ms = timer.elapsed();
      qDebug() << "Create OpenGLMesh (sec)      :" << float(ms) / 1e3f;
    }
    auto query =
      SELECT
        FROM ( edge : m_halfEdgeMesh->halfEdges() )
        WHERE ( edge.face == 0 )
        JOIN ( m_halfEdgeMesh->vertex(edge.to)->position,
               m_halfEdgeMesh->vertex(m_halfEdgeMesh->halfEdge(edge.next)->to)->position );
    {
      timer.start();
      m_boundaries = query();
      ms = timer.elapsed();
      qDebug() << "Mesh Query Time (sec)        :" << float(ms) / 1e3f;
    }
    {
      delete m_staticGeometryTopDown500;
      delete m_staticGeometryTopDown7;
      delete m_staticGeometryBottomUp7;
      delete m_staticGeometryBottomUp500;
      m_staticGeometryTopDown500 = new KStaticGeometry();
      m_staticGeometryTopDown7 = new KStaticGeometry();
      m_staticGeometryBottomUp7 = new KStaticGeometry();
      m_staticGeometryBottomUp500 = new KStaticGeometry();
      KTransform3D geomTrans;
      for (int i = 0; i < 4; ++i)
      {
        const float radius = 10.0f;
        float radians = i * Karma::TwoPi / 4.0f;
        geomTrans.setTranslation(std::cos(radians) * radius, 0.0f, std::sin(radians) * radius);
        m_staticGeometryTopDown500->addGeometry(*m_halfEdgeMesh, geomTrans);
        m_staticGeometryTopDown7->addGeometry(*m_halfEdgeMesh, geomTrans);
        m_staticGeometryBottomUp7->addGeometry(*m_halfEdgeMesh, geomTrans);
        m_staticGeometryBottomUp500->addGeometry(*m_halfEdgeMesh, geomTrans);
      }
      m_staticGeometry = 0;
    }
    qDebug() << "--------------------------------------";
    qDebug() << "Mesh Vertexes  :" << m_halfEdgeMesh->vertices().size();
    qDebug() << "Mesh Faces     :" << m_halfEdgeMesh->faces().size();
    qDebug() << "Mesh HalfEdges :" << m_halfEdgeMesh->halfEdges().size();
    qDebug() << "Boundary Edges :" << m_boundaries.size();
    qDebug() << "Polygons /Frame:" << m_halfEdgeMesh->faces().size() * m_instances.size();
  }

  m_paused = oldValue;
}

void MainWidgetPrivate::openObj()
{
  QString fileName = QFileDialog::getOpenFileName(
    m_parent, m_parent->tr("Open Model"),
    ".",
    m_parent->tr("Wavefront Object File (*.obj))")
  );
  if (!fileName.isNull())
  {
    loadObj(fileName);
  }
}

void MainWidgetPrivate::resizeGL(int width, int height)
{
  // Calculate the new render information
  float depthNear = 0.1f;
  float depthFar  = 1000.0f;
  KMatrix4x4 perspective;
  perspective.perspective(45.0f, width / float(height), depthNear, depthFar);

  // Update renderblocks
  OpenGLRenderBlock &currRenderBlock = m_renderBlocks[m_renderBlockIndex[0]];
  OpenGLRenderBlock &prevRenderBlock = m_renderBlocks[m_renderBlockIndex[1]];
  currRenderBlock.setNearFar(depthNear, depthFar);
  currRenderBlock.setPerspectiveMatrix(perspective);
  currRenderBlock.setDimensions(width, height);
  prevRenderBlock.setNearFar(depthNear, depthFar);
  prevRenderBlock.setPerspectiveMatrix(perspective);
  prevRenderBlock.setDimensions(width, height);

  for (OpenGLRenderPass *pass : m_passes)
  {
    pass->resize(width, height);
  }
}

void MainWidgetPrivate::paintGL()
{
  OpenGLProfiler::BeginFrame();
  {
    OpenGLMarkerScoped _("Total Render Time");
    commitGL();
    renderGL();
  }
  OpenGLProfiler::EndFrame();
  OpenGLDebugDraw::draw();
}

void MainWidgetPrivate::renderGL()
{
  for (OpenGLRenderPass *pass : m_passes)
  {
    // Todo: Pass Geometry Manager
    pass->render(*this);
  }
  composeScene();

  // Draw Bounding Volumes
  for (OpenGLInstance *i : m_instances)
  {
    if (b_bv[0]) m_aabbBV->draw(i->currentTransform(), Qt::red);
    if (b_bv[1]) m_sphereCentroidBV->draw(i->currentTransform(), Qt::red);
    if (b_bv[2]) m_sphereRittersBV->draw(i->currentTransform(), Qt::green);
    if (b_bv[3]) m_sphereLarssonsBV->draw(i->currentTransform(), Qt::blue);
    if (b_bv[4]) m_spherePcaBV->draw(i->currentTransform(), Qt::yellow);
    if (b_bv[5]) m_ellipsoidPcaBV->draw(i->currentTransform(), Qt::red);
    if (b_bv[6]) m_orientedPcaBV->draw(i->currentTransform(), Qt::red);
  }
  if (m_staticGeometry)
    m_staticGeometry->drawAabbs(KTransform3D(), Qt::red, m_minDraw, m_maxDraw);
}

void MainWidgetPrivate::composeScene()
{
  OpenGLMarkerScoped _("Composition Pass");
  m_deferredPrograms[m_presentation]->bind();
  m_quadGL.draw();
  m_deferredPrograms[m_presentation]->release();
}

void MainWidgetPrivate::commitGL()
{
  OpenGLMarkerScoped _("Prepare Scene");

  // Update the previous/current render block bindings
  if (m_camera.dirty())
  {
    swapRenderBlocks();
    currentRenderBlock().setViewMatrix(m_camera.toMatrix());
  }
  else
  {
    fixRenderBlocks();
  }
  updateRenderBlocks();

  // Update the GPU instance data
  OpenGLRenderBlock &currRenderBlock = currentRenderBlock();
  OpenGLRenderBlock &prevRenderBlock = previousRenderBlock();
  m_instanceGroup.update(currRenderBlock, prevRenderBlock);
  m_floorGroup.update(currRenderBlock, prevRenderBlock);
  for (OpenGLRenderPass *pass : m_passes)
  {
    pass->commit(currRenderBlock, prevRenderBlock);
  }
}

OpenGLRenderBlock &MainWidgetPrivate::currentRenderBlock()
{
  return m_renderBlocks[m_renderBlockIndex[0]];
}

OpenGLRenderBlock &MainWidgetPrivate::previousRenderBlock()
{
  if (OpenGLUniformBufferObject::boundBufferId(1) != OpenGLUniformBufferObject::boundBufferId(2))
  {
    return m_renderBlocks[m_renderBlockIndex[1]];
  }
  return m_renderBlocks[m_renderBlockIndex[0]];
}

void MainWidgetPrivate::swapRenderBlocks()
{
  // Get the render blocks in their current state
  OpenGLRenderBlock &currRenderBlock = m_renderBlocks[m_renderBlockIndex[0]];
  OpenGLRenderBlock &prevRenderBlock = m_renderBlocks[m_renderBlockIndex[1]];

  // Swap the binding indices of the render blocks
  std::swap(m_renderBlockIndex[0], m_renderBlockIndex[1]);

  // Update the binding indices of each render block
  currRenderBlock.bindBase(m_renderBlockIndex[0] + 1);
  prevRenderBlock.bindBase(m_renderBlockIndex[1] + 1);
}

void MainWidgetPrivate::fixRenderBlocks()
{
  // Current = Previous (No camera motion applied)
  if (OpenGLUniformBufferObject::boundBufferId(1) != OpenGLUniformBufferObject::boundBufferId(2))
  {
    OpenGLRenderBlock &currRenderBlock = m_renderBlocks[m_renderBlockIndex[0]];
    currRenderBlock.bindBase(m_renderBlockIndex[0] + 1);
  }
}

void MainWidgetPrivate::updateRenderBlocks()
{
  // Update previous/current render block data (if needed)
  for (int i = 0; i < 2; ++i)
  {
    if (m_renderBlocks[i].dirty())
    {
      m_renderBlocks[i].bind();
      m_renderBlocks[i].update();
      m_renderBlocks[i].release();
    }
  }
}

void MainWidgetPrivate::renderGeometry()
{
  m_instanceGroup.draw();
  m_floorGroup.draw();
}

/*******************************************************************************
 * MainWidget
 ******************************************************************************/
MainWidget::MainWidget(QWidget *parent) :
  OpenGLWidget(parent)
{
  // Set Shader Includes
  OpenGLShaderProgram::addSharedIncludePath(":/resources/shaders");
  OpenGLShaderProgram::addSharedIncludePath(":/resources/shaders/ubo");
}

MainWidget::~MainWidget()
{
  makeCurrent();
  teardownGL();
  delete m_private;
}

/*******************************************************************************
 * OpenGL Methods
 ******************************************************************************/
void MainWidget::initializeGL()
{
  m_private = new MainWidgetPrivate(this);
  P(MainWidgetPrivate);
  p.m_dragVelocity = 0.0f;

  p.initializeGL();
  OpenGLWidget::initializeGL();
  printVersionInformation();

  // Set global information
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glClearDepth(1.0f);
  glDepthFunc(GL_LEQUAL);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

  p.m_quad = new KHalfEdgeMesh(this, ":/resources/objects/quad.obj");
  p.m_quadGL.create(*p.m_quad);

  // Application-specific initialization
  {
    // Uniform Block Object
    for (int i = 0; i < 2; ++i)
    {
      p.m_renderBlocks[i].create();
      p.m_renderBlocks[i].setUsagePattern(OpenGLBuffer::DynamicDraw);
      p.m_renderBlocks[i].bind();
      p.m_renderBlocks[i].allocate();
      p.m_renderBlocks[i].release();
      p.m_renderBlocks[i].setViewMatrix(p.m_camera.toMatrix());
    }
    p.m_renderBlocks[0].bindBase(1);
    p.m_renderBlocks[0].bindBase(2);

    char const* fragFiles[] = {
      ":/resources/shaders/gbuffer/backbuffer.frag",
      ":/resources/shaders/gbuffer/depth.frag",
      ":/resources/shaders/gbuffer/linearDepth.frag",
      ":/resources/shaders/gbuffer/position.frag",
      ":/resources/shaders/gbuffer/normal.frag",
      ":/resources/shaders/gbuffer/diffuse.frag",
      ":/resources/shaders/gbuffer/specular.frag",
      ":/resources/shaders/gbuffer/velocity.frag",
      ":/resources/shaders/gbuffer/lightbuffer.frag"
    };

    if (sizeof(fragFiles) / sizeof(char const*) != MaxPresentations)
    {
      qFatal("Fatal: Must be able to present screen data for every presentation type!");
    }

    for (int i = 0; i < MaxPresentations; ++i)
    {
      p.m_deferredPrograms[i] = new OpenGLShaderProgram(this);
      p.m_deferredPrograms[i]->addIncludePath(":/resources/shaders");
      p.m_deferredPrograms[i]->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/resources/shaders/gbuffer/main.vert");
      p.m_deferredPrograms[i]->addShaderFromSourceFile(QOpenGLShader::Fragment, fragFiles[i]);
      p.m_deferredPrograms[i]->link();
    }

    // Initialize the Direction Light Group
    for (int i = 0; i < 1; ++i)
    {
      OpenGLDirectionLight *light = p.m_lightPass->createDirectionLight();
      light->setDiffuse(0.1f, 0.1f, 0.1f);
      light->setSpecular(0.1f, 0.1f, 0.1f);
    }

    // Initialize the Point Light Group
    for (int i = 0; i < 5; ++i)
    {
      OpenGLPointLight *light = p.m_lightPass->createPointLight();
      light->setRadius(25.0f);
    }

    // Initialize the Spot Light Group
    for (int i = 0; i < 3; ++i)
    {
      OpenGLSpotLight *light = p.m_lightPass->createSpotLight();
      light->setInnerAngle(40.0f);
      light->setOuterAngle(45.0f);
      light->setDepth(25.0f);
    }

    p.m_floorGroup.create();
    p.m_instanceGroup.create();
    // Open OBJ
    KHalfEdgeMesh *mesh = new KHalfEdgeMesh(this, ":/resources/objects/floor.obj");
    mesh->calculateVertexNormals();
    p.m_floorGL.create(*mesh);
    p.m_floorGroup.setMesh(p.m_floorGL);
    p.m_floorInstance = p.m_floorGroup.createInstance();
    p.m_floorInstance->material().setDiffuse(0.0f, 0.0f, 1.0f);
    p.m_floorInstance->material().setSpecular(0.25f, 0.25f, 0.25f, 1.0f);
    p.m_floorInstance->transform().setScale(1000.0f);
    p.m_floorInstance->transform().setTranslation(0.0f, -1.0f, 0.0f);
    p.loadObj(":/resources/objects/sphere.obj");

    // Initialize instances
    //*
    for (int i = 0; i < 4; ++i)
    {
      const float radius = 10.0f;
      float radians = i * Karma::TwoPi / 4.0f;
      OpenGLInstance * instance = p.m_instanceGroup.createInstance();
      instance->currentTransform().setScale(1.0f);
      instance->material().setDiffuse(0.0f, 1.0f, 0.0f);
      instance->material().setSpecular(1.0f, 1.0f, 1.0f, 32.0f);
      instance->currentTransform().setTranslation(std::cos(radians) * radius, 0.0f, std::sin(radians) * radius);
    }
    OpenGLInstance * instance = p.m_instanceGroup.createInstance();
    instance->currentTransform().setScale(1.0f);
    instance->material().setDiffuse(0.0f, 1.0f, 0.0f);
    instance->material().setSpecular(1.0f, 1.0f, 1.0f, 32.0f);
    p.m_instances.push_back(instance);
    //*/
  }

  OpenGLDebugDraw::initialize();
}

void MainWidget::resizeGL(int width, int height)
{
  P(MainWidgetPrivate);
  p.resizeGL(width, height);
  OpenGLWidget::resizeGL(width, height);
  OpenGLFramebufferObject::setRelease(defaultFramebufferObject());
}

void MainWidget::paintGL()
{
  P(MainWidgetPrivate);

  if (!p.m_paused)
  {
    p.paintGL();
    OpenGLWidget::paintGL();
  }
}

void MainWidget::teardownGL()
{
  OpenGLDebugDraw::teardown();
  OpenGLWidget::teardownGL();
}

/*******************************************************************************
 * Events
 ******************************************************************************/
void MainWidget::updateEvent(KUpdateEvent *event)
{
  P(MainWidgetPrivate);
  (void)event;

  // Update instances

  static float f = 0.0f;
  f += 0.0016f;
  float angle = f;
  for (OpenGLDirectionLight *light : p.m_lightPass->directionLights())
  {
    light->setDirection(std::cos(angle), -1, std::sin(angle));
  }
  for (OpenGLPointLight *instance : p.m_lightPass->pointLights())
  {
    static const float radius = 5.0f;
    instance->setTranslation(cos(angle) * radius, 0.0f, sin(angle) * radius);
    angle += 2 * 3.1415926 / p.m_lightPass->pointLights().size();
  }
  angle = f;

  for (OpenGLSpotLight *instance : p.m_lightPass->spotLights())
  {
    static const float radius = 5.0f;
    instance->setTranslation(cos(angle) * radius, 5.0f + std::sin(angle * 15.0f) * 5.0f, sin(angle) * radius);
    instance->setDirection(-instance->translation().normalized());
    angle += 2 * 3.1415926 / p.m_lightPass->spotLights().size();
  }

  if (KInputManager::keyTriggered(Qt::Key_Plus))
  {
    for (OpenGLInstance *instance : p.m_instances)
    {
      instance->currentTransform().grow(1.0f);
    }
  }

  if (KInputManager::keyTriggered(Qt::Key_Underscore))
  {
    for (OpenGLInstance *instance : p.m_instances)
    {
      instance->currentTransform().grow(-1.0f);
    }
  }

  bool triggered = false;
  if (KInputManager::keyTriggered(Qt::Key_BracketLeft))
  {
    --p.m_maxDraw;
    triggered = true;
  }

  if (KInputManager::keyTriggered(Qt::Key_BracketRight))
  {
    ++p.m_maxDraw;
    triggered = true;
  }


  if (KInputManager::keyTriggered(Qt::Key_BraceLeft))
  {
    --p.m_minDraw;
    triggered = true;
  }

  if (KInputManager::keyTriggered(Qt::Key_BraceRight))
  {
    ++p.m_minDraw;
    triggered = true;
  }

  if (p.m_staticGeometry)
  {
    if (p.m_minDraw < 0)
    {
      p.m_minDraw = 0;
    }
    if (p.m_minDraw > p.m_staticGeometry->depth())
    {
      p.m_minDraw = static_cast<int>(p.m_staticGeometry->depth());
    }
    if (p.m_maxDraw < p.m_minDraw)
    {
      p.m_maxDraw = p.m_minDraw;
    }
    if (p.m_maxDraw > p.m_staticGeometry->depth())
    {
      p.m_maxDraw = static_cast<int>(p.m_staticGeometry->depth());
    }

    if (triggered)
    {
      QString format("MinMaxBounds [%1,%2]");
      QMainWindow* window = NULL;
      foreach(QWidget *widget, qApp->topLevelWidgets())
      {
        if(widget->inherits("QMainWindow"))
        {
          window = static_cast<QMainWindow*>(widget);
          window->setWindowTitle( format.arg(p.m_minDraw).arg(p.m_maxDraw) );
          break;
        }
      }
    }
  }

  for (OpenGLInstance *instance : p.m_instances)
  {
    if (p.b_rZ) instance->currentTransform().rotate(0.5f, 0.0f, 0.0f, 1.0f);
    if (p.b_rY) instance->currentTransform().rotate(0.25f, 0.0f, 1.0f, 0.0f);
    if (p.b_rX) instance->currentTransform().rotate(-1.25f, 1.0f, 0.0f, 0.0f);
  }

  // Camera Transformation
  if (KInputManager::buttonPressed(Qt::RightButton))
  {
    float transSpeed = 3.0f;
    float rotSpeed   = 0.5f;

    if (KInputManager::keyPressed(Qt::Key_Control))
    {
      transSpeed = 1.0f;
    }

    // Handle rotations
    p.m_camera.rotate(-rotSpeed * KInputManager::mouseDelta().x(), KCamera3D::LocalUp);
    p.m_camera.rotate(-rotSpeed * KInputManager::mouseDelta().y(), p.m_camera.right());

    // Handle translations
    QVector3D translation;
    if (KInputManager::keyPressed(Qt::Key_W))
    {
      translation += p.m_camera.forward();
    }
    if (KInputManager::keyPressed(Qt::Key_S))
    {
      translation -= p.m_camera.forward();
    }
    if (KInputManager::keyPressed(Qt::Key_A))
    {
      translation -= p.m_camera.right();
    }
    if (KInputManager::keyPressed(Qt::Key_D))
    {
      translation += p.m_camera.right();
    }
    if (KInputManager::keyPressed(Qt::Key_E))
    {
      translation -= p.m_camera.up();
    }
    if (KInputManager::keyPressed(Qt::Key_Q))
    {
      translation += p.m_camera.up();
    }
    p.m_camera.translate(transSpeed * translation);
  }
  else
  {
    if (KInputManager::keyTriggered(Qt::Key_X)) p.b_rX = !p.b_rX;
    if (KInputManager::keyTriggered(Qt::Key_Y)) p.b_rY = !p.b_rY;
    if (KInputManager::keyTriggered(Qt::Key_Z)) p.b_rZ = !p.b_rZ;
  }

  if (KInputManager::keyPressed(Qt::Key_Control))
  {
    if (KInputManager::keyTriggered(Qt::Key_O))
    {
      p.openObj();
    }
  }

  if (KInputManager::keyPressed(Qt::Key_Shift))
  {
    auto depthPred = [](size_t numTriangles, size_t depth)->bool { (void)numTriangles; return depth >= 7; };
    if (KInputManager::keyTriggered(Qt::Key_B))
    {
      p.m_staticGeometry = p.m_staticGeometryBottomUp7;
      p.m_staticGeometryBottomUp7->build(KStaticGeometry::BottomUpMethod, depthPred);
      p.m_maxDraw = static_cast<int>(p.m_staticGeometry->depth());
    }
    if (KInputManager::keyTriggered(Qt::Key_T))
    {
      p.m_staticGeometry = p.m_staticGeometryTopDown7;
      p.m_staticGeometryTopDown7->build(KStaticGeometry::TopDownMethod, depthPred);
      p.m_maxDraw = static_cast<int>(p.m_staticGeometry->depth());
    }
  }
  else
  {
    auto trianglePred = [](size_t numTriangles, size_t depth)->bool { (void)depth; return numTriangles < 500; };
    if (KInputManager::keyTriggered(Qt::Key_B))
    {
      p.m_staticGeometry = p.m_staticGeometryBottomUp500;
      p.m_staticGeometryBottomUp500->build(KStaticGeometry::BottomUpMethod, trianglePred);
      p.m_maxDraw = static_cast<int>(p.m_staticGeometry->depth());
    }
    if (KInputManager::keyTriggered(Qt::Key_T))
    {
      p.m_staticGeometry = p.m_staticGeometryTopDown500;
      p.m_staticGeometryTopDown500->build(KStaticGeometry::TopDownMethod, trianglePred);
      p.m_maxDraw = static_cast<int>(p.m_staticGeometry->depth());
    }
  }

  // Change Buffer
  if (KInputManager::keyPressed(Qt::Key_Shift))
  {
    if (KInputManager::keyTriggered(Qt::Key_ParenRight))
    {
      p.m_presentation = PresentComposition;
    }
    if (KInputManager::keyTriggered(Qt::Key_Exclam))
    {
      p.m_presentation = PresentDepth;
    }
    if (KInputManager::keyTriggered(Qt::Key_At))
    {
      p.m_presentation = PresentLinearDepth;
    }
    if (KInputManager::keyTriggered(Qt::Key_NumberSign))
    {
      p.m_presentation = PresentPosition;
    }
    if (KInputManager::keyTriggered(Qt::Key_Dollar))
    {
      p.m_presentation = PresentViewNormal;
    }
    if (KInputManager::keyTriggered(Qt::Key_Percent))
    {
      p.m_presentation = PresentDiffuse;
    }
    if (KInputManager::keyTriggered(Qt::Key_AsciiCircum))
    {
      p.m_presentation = PresentSpecular;
    }
    if (KInputManager::keyTriggered(Qt::Key_Ampersand))
    {
      p.m_presentation = PresentVelocity;
    }
    if (KInputManager::keyTriggered(Qt::Key_Asterisk))
    {
      p.m_presentation = PresentLightAccumulation;
    }
  }
  else
  {
    if (KInputManager::keyTriggered(Qt::Key_0))
    {
      p.b_bv[0] = !p.b_bv[0];
    }
    if (KInputManager::keyTriggered(Qt::Key_1))
    {
      p.b_bv[1] = !p.b_bv[1];
    }
    if (KInputManager::keyTriggered(Qt::Key_2))
    {
      p.b_bv[2] = !p.b_bv[2];
    }
    if (KInputManager::keyTriggered(Qt::Key_3))
    {
      p.b_bv[3] = !p.b_bv[3];
    }
    if (KInputManager::keyTriggered(Qt::Key_4))
    {
      p.b_bv[4] = !p.b_bv[4];
    }
    if (KInputManager::keyTriggered(Qt::Key_5))
    {
      p.b_bv[5] = !p.b_bv[5];
    }
    if (KInputManager::keyTriggered(Qt::Key_6))
    {
      p.b_bv[6] = !p.b_bv[6];
    }
  }

  // Pinching will grow/shrink
  KPinchGesture pinch;
  if (KInputManager::pinchGesture(&pinch))
  {
    //p.m_transform.scale(pinch.scaleFactor());
    //p.m_transform.rotate(pinch.lastRotationAngle() - pinch.rotationAngle(), 0.0f, 0.0f, 1.0f);
  }

  // Panning will translate
  KPanGesture pan;
  if (KInputManager::panGesture(&pan))
  {
    KVector3D delta = KVector3D(pan.delta().x(), -pan.delta().y(), 0.0f) * 0.1f;
    //p.m_transform.translate(delta);
  }

  // Touching will rotate
  if (KInputManager::touchCount() == 1)
  {
    KTouchPoint touch = KInputManager::touchPoint(0);
    KPointF delta = touch.pos() - touch.lastPos();
    KVector3D axis(delta.y(), delta.x(), 0.0f);
    switch (touch.state())
    {
    case Qt::TouchPointPressed:
      p.m_dragVelocity = 0.0f;
      break;
    case Qt::TouchPointMoved:
      p.m_dragAxis = p.m_camera.rotation().rotatedVector(axis);
      p.m_dragVelocity = axis.length() * 0.1f;
      p.m_dragAxis.normalize();
      break;
    default:
      break;
    }
  }

  // Rotate from drag gesture
  p.m_dragVelocity *= 0.9f;
  //p.m_transform.rotate(p.m_dragVelocity, p.m_dragAxis);
}

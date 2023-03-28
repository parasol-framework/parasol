<!-- xsltproc -o audio.html class.xsl audio.xml -->
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output doctype-public="-//W3C//DTD XHTML 1.0 Strict//EN" media-type="application/html+xml" encoding="utf-8" omit-xml-declaration="yes" indent="no"/>

  <xsl:template match="types">
    <xsl:choose>
      <xsl:when test="type"> <!-- Build a one-off table of constant values -->
        <table class="table">
          <thead><tr><th>Name</th><th>Description</th></tr></thead>
          <tbody>
            <xsl:for-each select="type">
              <xsl:choose>
                <xsl:when test="../@prefix">
                  <tr><th class="col-md-1"><xsl:value-of select="../@prefix"/>_<xsl:value-of select="@name"/></th><td><xsl:value-of select="."/></td></tr>
                </xsl:when>
                <xsl:otherwise>
                  <tr><th class="col-md-1"><xsl:value-of select="@name"/></th><td><xsl:value-of select="."/></td></tr>
                </xsl:otherwise>
              </xsl:choose>
            </xsl:for-each>
          </tbody>
        </table>
      </xsl:when>
      <xsl:when test="@lookup"> <!-- Build a table of constant values from the dictionary -->
        <xsl:variable name="prefix"><xsl:value-of select="@lookup"/></xsl:variable>
        <table class="table">
          <thead><tr><th class="col-md-1">Name</th><th>Description</th></tr></thead>
          <tbody>
            <xsl:for-each select="/book/types/constants[@lookup=$prefix]/const">
              <tr><th class="col-md-1"><xsl:value-of select="$prefix"/>_<xsl:value-of select="@name"/></th><td><xsl:value-of select="."/></td></tr>
            </xsl:for-each>
          </tbody>
        </table>
      </xsl:when>
      <xsl:otherwise>
        <p class="text-danger">Incorrect types tag detected.</p>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="p">
    <xsl:copy>
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates select="*|text()" />
    </xsl:copy>
  </xsl:template>

  <xsl:template match="text()"><xsl:value-of select="."/></xsl:template>

  <xsl:template match="field"><xsl:variable name="fieldName"><xsl:value-of select="node()"/></xsl:variable>
    <a data-toggle="tooltip"><xsl:attribute name="title"><xsl:value-of select="/book/fields/field[name=$fieldName]/comment"/></xsl:attribute><xsl:attribute name="href">#tf-<xsl:value-of select="node()"/></xsl:attribute><xsl:value-of select="node()"/></a></xsl:template>

  <xsl:template match="action"><xsl:variable name="actionName"><xsl:value-of select="node()"/></xsl:variable>
    <a data-toggle="tooltip"><xsl:attribute name="title"><xsl:value-of select="/book/actions/action[name=$actionName]/comment"/></xsl:attribute><xsl:attribute name="href">#ta-<xsl:value-of select="node()"/></xsl:attribute><xsl:value-of select="node()"/>()</a></xsl:template>

  <xsl:template match="method"><xsl:variable name="methodName"><xsl:value-of select="node()"/></xsl:variable>
    <a data-toggle="tooltip"><xsl:attribute name="title"><xsl:value-of select="/book/methods/method[name=$methodName]/comment"/></xsl:attribute><xsl:attribute name="href">#tm-<xsl:value-of select="node()"/></xsl:attribute><xsl:value-of select="node()"/>()</a></xsl:template>

  <xsl:template match="class">
    <a><xsl:attribute name="href"><xsl:value-of select="@name"/>.xml<xsl:if test="@index">?index=<xsl:value-of select="@index"/></xsl:if></xsl:attribute><xsl:value-of select="@name"/>
      <xsl:if test="@index">&#8658;<xsl:value-of select="@index"/></xsl:if>
    </a>
  </xsl:template>

  <xsl:template match="list">
    <xsl:choose>
      <xsl:when test="@type='ordered'">
        <ol><xsl:apply-templates select="*|node()"/></ol>
      </xsl:when>
      <xsl:otherwise>
        <ul><xsl:apply-templates select="*|node()"/></ul>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="li">
    <xsl:copy>
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates select="*|text()" />
    </xsl:copy>
  </xsl:template>

  <xsl:template match="include">
    <code><xsl:value-of select="."/></code>
  </xsl:template>

  <xsl:template match="header">
    <h3><xsl:value-of select="."/></h3>
  </xsl:template>

  <xsl:template match="code">
    <xsl:copy-of select="."/>
  </xsl:template>

  <xsl:template match="pre">
    <xsl:copy-of select="."/>
  </xsl:template>

  <xsl:template name="addGoogleTracking">
    <!-- Global site tag (gtag.js) - Google Analytics -->
	 <script async="async" src="https://www.googletagmanager.com/gtag/js?id=G-8254DG7MT6"><xsl:text> </xsl:text></script>
	 <script>
	   <xsl:text disable-output-escaping="yes">
	   window.dataLayer = window.dataLayer || [];
	   function gtag(){dataLayer.push(arguments);}
	   gtag('js', new Date());
      gtag('config', 'G-8254DG7MT6');</xsl:text>
	 </script>
  </xsl:template>

  <xsl:template match="/book">
    <html xml:lang="en">
      <head>
        <xsl:call-template name="addGoogleTracking"/>
        <meta charset="utf-8"/>
        <meta http-equiv="X-UA-Compatible" content="IE=edge"/>
        <meta name="viewport" content="width=device-width, initial-scale=1"/>
        <!-- The above 3 meta tags *must* come first in the head; any other head content must come *after* these tags -->
        <meta name="description" content="Parasol Framework documentation, machine generated from source"/>
        <meta name="author" content="Paul Manias"/>
        <link rel="icon" href="/favicon.ico"/>
        <title>Parasol Framework Manual</title>
        <!-- Bootstrap core CSS -->
        <link href="../../css/bootstrap.min.css" rel="stylesheet"/>
        <!-- Custom styles for this template -->
        <link href="../../css/module-template.css" rel="stylesheet"/>

        <script>
          var shiftWindow = function() { scrollBy(0, -100) };
          window.addEventListener("hashchange", shiftWindow);
          function load() { if (window.location.hash) shiftWindow(); }
        </script>
        <!-- HTML5 shim and Respond.js for IE8 support of HTML5 elements and media queries -->
        <!--[if lt IE 9]>
          <script src="https://oss.maxcdn.com/html5shiv/3.7.3/html5shiv.min.js"></script>
          <script src="https://oss.maxcdn.com/respond/1.4.2/respond.min.js"></script>
        <![endif]-->
      </head>

      <body>
        <nav class="navbar navbar-inverse navbar-fixed-top">
          <div class="container">
            <div class="navbar-header">
              <button type="button" class="navbar-toggle collapsed" data-toggle="collapse" data-target="#navbar" aria-expanded="false" aria-controls="navbar">
                <span class="sr-only">Toggle navigation</span>
                <span class="icon-bar"></span>
                <span class="icon-bar"></span>
                <span class="icon-bar"></span>
              </button>
              <a class="navbar-brand" href="/index.html">Parasol Framework</a>
            </div>
            <div id="navbar" class="collapse navbar-collapse">
              <ul class="nav navbar-nav">
                <li><a href="../core.xml">Modules</a></li>
                <li class="active"><a href="classes.html">Classes</a></li>
                <li><a href="../fluid.html">Fluid</a></li>
              </ul>
            </div> <!-- nav-collapse -->
          </div>
        </nav>

        <div id="container-body" class="container"> <!-- Use container-fluid if you want full width -->
          <div id="row-body" class="row">
            <div class="col-sm-9">
              <div class="docs-content" id="default-page">
                <div class="page-header"><h1><xsl:value-of select="/book/info/name"/> Class</h1></div>
                <p class="lead"><xsl:value-of select="/book/info/comment"/></p>
                <xsl:for-each select="/book/info/description">
                  <xsl:apply-templates/>
                </xsl:for-each>

                <h3>Structure</h3>
                <p>The <xsl:value-of select="info/name"/> class consists of the following fields:</p>
                <table class="table table-hover">
                  <thead><th class="col-md-1"><div data-toggle="tooltip"  title="Read/Write access indicators are listed below">Access</div></th><th class="col-md-1">Name</th><th class="col-md-1">Type</th><th>Comment</th></thead>
                  <tbody>
                    <xsl:for-each select="/book/fields/field">
                      <tr id="_" data-toggle="collapse" data-target="_" class="clickable">
                        <xsl:attribute name="data-target">#fl-<xsl:value-of select="name"/></xsl:attribute>
                        <td class="col-md-1">
                          <a><xsl:attribute name="id">tf-<xsl:value-of select="name"/></xsl:attribute></a>
                          <xsl:choose>
                            <xsl:when test="access/@read='R'"><span class="glyphicon glyphicon-ok text-success" data-toggle="tooltip"  title="Direct read access"/></xsl:when>
                            <xsl:when test="access/@read='G'"><span class="glyphicon glyphicon-ok text-danger" data-toggle="tooltip" title="Functional read access"/></xsl:when>
                            <xsl:otherwise><span class="glyphicon glyphicon-minus text-muted" data-toggle="tooltip" title="Not readable"/></xsl:otherwise>
                          </xsl:choose>
                          &#160;
                          <xsl:choose>
                            <xsl:when test="access/@write='W'"><span class="glyphicon glyphicon-ok text-success" data-toggle="tooltip" title="Direct write access"/></xsl:when>
                            <xsl:when test="access/@write='S'"><span class="glyphicon glyphicon-ok text-danger" data-toggle="tooltip" title="Functional write access"/></xsl:when>
                            <xsl:when test="access/@write='I'"><span class="glyphicon glyphicon-cog text-danger" data-toggle="tooltip" title="Immutable"/></xsl:when>
                            <xsl:otherwise><span class="glyphicon glyphicon-minus text-muted" data-toggle="tooltip" title="Not writeable"/></xsl:otherwise>
                          </xsl:choose></td>
                        <th class="col-md-1"><xsl:value-of select="name"/></th>
                        <td class="col-md-1"><span class="text-nowrap">
                          <xsl:choose>
                            <xsl:when test="type/@class">
                              <a><xsl:attribute name="href"><xsl:value-of select="type/@class"/>.xml</xsl:attribute><xsl:value-of select="type"/></a>
                            </xsl:when>
                            <xsl:otherwise>
                              <xsl:value-of select="type"/>
                            </xsl:otherwise>
                          </xsl:choose>
                        </span></td>
                        <td><xsl:value-of select="comment"/></td>
                      </tr>
                      <xsl:if test="description">
                        <tr class="no-hover">
                          <td colspan="4" class="hiddenRow">
                            <div id="_" class="accordion-body collapse">
                              <xsl:attribute name="id">fl-<xsl:value-of select="name"/></xsl:attribute>
                              <div class="doc-content" style="margin:20px">
                                <xsl:for-each select="description">
                                  <xsl:apply-templates/>
                                </xsl:for-each>
                              </div>
                            </div>
                          </td>
                        </tr>
                      </xsl:if>
                    </xsl:for-each>
                  </tbody>
                </table>

                <xsl:if test="/book/actions/action">
                  <h3>Actions</h3>
                  <p>The following actions are currently supported:</p>
                  <table class="table table-hover">
                    <thead><th style="width:1%; border-top-style: none; border-bottom-style: none;"/><th class="col-md-1">Name</th><th>Comment</th></thead>
                    <tbody>
                      <xsl:for-each select="/book/actions/action">
                        <tr data-toggle="collapse" data-target="_" class="clickable">
                          <xsl:attribute name="data-target">#fl-<xsl:value-of select="name"/></xsl:attribute>
                          <td style="width:1%; border-top-style: none; border-bottom-style: none;">
                            <a><xsl:attribute name="id">ta-<xsl:value-of select="name"/></xsl:attribute></a>
                            <xsl:choose>
                              <xsl:when test="description">
                                <span class="glyphicon glyphicon-chevron-right"/>
                              </xsl:when>
                            </xsl:choose>
                          </td>
                          <th class="col-md-1"><a href="actions.xml"><xsl:value-of select="name"/></a></th>
                          <td><xsl:value-of select="comment"/></td>
                        </tr>
                        <xsl:if test="description">
                          <tr class="no-hover">
                            <td class="hiddenRow" style="width:1%; border-top-style: none; border-bottom-style: none;"/>
                            <td colspan="2" class="hiddenRow">
                              <div id="_" class="accordion-body collapse">
                                <xsl:attribute name="id">fl-<xsl:value-of select="name"/></xsl:attribute>
                                <div class="docs-content" style="margin:10px 10px 0px 30px">
                                  <xsl:for-each select="description">
                                    <xsl:apply-templates/>
                                  </xsl:for-each>
                                  <xsl:choose>
                                    <xsl:when test="result/error">
                                      <h3>Error Codes</h3>
                                      <table class="table table-sm borderless">
                                        <tbody>
                                          <xsl:for-each select="result/error">
                                            <tr><th class="col-md-1"><xsl:value-of select="@code"/></th><td><xsl:value-of select="."/></td></tr>
                                          </xsl:for-each>
                                        </tbody>
                                      </table>
                                    </xsl:when>
                                    <xsl:when test="result">
                                      <h3>Result</h3>
                                      <p><xsl:value-of select="result/."/></p>
                                    </xsl:when>
                                  </xsl:choose>
                                </div>
                              </div>
                            </td>
                          </tr>
                        </xsl:if>
                      </xsl:for-each>
                    </tbody>
                  </table>
                </xsl:if>

                <xsl:if test="/book/methods/method">
                  <h3>Methods</h3>
                  <p>The following methods are currently supported:</p>
                  <table class="table table-hover">
                    <thead><th class="col-md-1">Name</th><th>Comment</th></thead>
                    <tbody>
                      <xsl:for-each select="/book/methods/method">
                        <tr data-toggle="collapse" data-target="_" class="clickable">
                          <xsl:attribute name="data-target">#fl-<xsl:value-of select="name"/></xsl:attribute>
                          <th class="col-md-1 text-primary"><xsl:value-of select="name"/></th>
                          <td><a><xsl:attribute name="id">tm-<xsl:value-of select="name"/></xsl:attribute></a><xsl:value-of select="comment"/></td>
                        </tr>
                        <tr class="no-hover">
                          <td colspan="2" class="hiddenRow">
                            <div id="_" class="accordion-body collapse">
                              <xsl:attribute name="id">fl-<xsl:value-of select="name"/></xsl:attribute>

                              <div class="panel panel-info" style="border-radius: 0;">
                                <div class="panel-heading" style="border-radius: 0;"><samp><xsl:value-of select="prototype"/></samp></div>
                                <xsl:choose>
                                  <xsl:when test="input/param">
                                    <div class="panel-body">
                                      <table class="table" style="border: 4px; margin-bottom: 0px; border: 0px; border-bottom: 0px;">
                                        <thead>
                                          <tr><th class="col-md-1">Input</th><th>Description</th></tr>
                                        </thead>
                                        <tbody>
                                          <xsl:for-each select="input/param">
                                            <xsl:choose>
                                              <xsl:when test="@lookup">
                                                <tr><td><a href="#"><xsl:attribute name="onclick">showPage('<xsl:value-of select="@lookup"/>');</xsl:attribute><xsl:value-of select="@name"/></a></td><td><xsl:value-of select="."/></td></tr>
                                              </xsl:when>
                                              <xsl:otherwise>
                                                <tr><td><xsl:value-of select="@name"/></td><td><xsl:value-of select="."/></td></tr>
                                              </xsl:otherwise>
                                            </xsl:choose>
                                          </xsl:for-each>
                                        </tbody>
                                      </table>
                                    </div>
                                  </xsl:when>
                                </xsl:choose>
                              </div>

                              <div class="docs-content" style="margin:30px 20px;">
                                <xsl:for-each select="description">
                                  <xsl:apply-templates/>
                                </xsl:for-each>

                                <xsl:choose>
                                  <xsl:when test="result/error">
                                    <h3>Error Codes</h3>
                                    <table class="table table-sm borderless">
                                      <tbody>
                                        <xsl:for-each select="result/error">
                                          <tr><th class="col-md-1"><xsl:value-of select="@code"/></th><td><xsl:value-of select="."/></td></tr>
                                        </xsl:for-each>
                                      </tbody>
                                    </table>
                                  </xsl:when>
                                  <xsl:when test="result">
                                    <h3>Result</h3>
                                    <p><xsl:value-of select="result/."/></p>
                                  </xsl:when>
                                </xsl:choose>
                              </div>
                            </div>
                          </td>
                        </tr>
                      </xsl:for-each>
                    </tbody>
                  </table>
                </xsl:if>
              </div>
              <div class="footer copyright text-right"><xsl:value-of select="/book/info/name"/> module documentation © <xsl:value-of select="/book/info/copyright"/></div>
            </div> <!-- End of core content -->

            <!-- SIDEBAR -->
            <div class="col-sm-3">
              <div id="nav-tree">
                <div class="panel-group" id="accordion">
                  <table class="table ">
                  <thead><th colspan="2"><h4>Class Info</h4></th></thead>
                    <tbody>
                      <tr><th class="col-md-1 text-primary">ID</th><td><xsl:value-of select="/book/info/idstring"/></td></tr>
                      <tr><th class="col-md-1 text-primary">Category</th><td><xsl:value-of select="/book/info/category"/></td></tr>
                      <tr><th class="col-md-1 text-primary">Include</th><td><xsl:value-of select="/book/info/include"/></td></tr>
                      <tr><th class="col-md-1 text-primary">Version</th><td><xsl:value-of select="/book/info/version"/></td></tr>
                    </tbody>
                  </table>

                  <div class="panel panel-primary">
                    <div class="panel-heading">
                      <h4 class="panel-title">Class List</h4>
                    </div>
                    <div id="structures" class="panel-collapse">
                      <div class="panel-body">
                        <ul class="list-unstyled">
                          <li>Audio<ul><li><a href="audio.xml">Audio</a></li><li><a href="sound.xml">Sound</a></li></ul></li>
                          <li>Core<ul><li><a href="file.xml">File</a></li><li><a href="metaclass.xml">MetaClass</a></li><li><a href="module.xml">Module</a></li><li><a href="storagedevice.xml">StorageDevice</a></li><li><a href="task.xml">Task</a></li><li><a href="thread.xml">Thread</a></li><li><a href="time.xml">Time</a></li></ul></li>
                          <li>Data<ul><li><a href="compression.xml">Compression</a></li><li><a href="config.xml">Config</a></li><li><a href="script.xml">Script</a></li><li><a href="xml.xml">XML</a></li></ul></li>
                          <li>Extensions<ul><li><a href="scintilla.xml">Scintilla</a></li><li><a href="scintillasearch.xml">ScintillaSearch</a></li></ul></li>
                          <li>Graphics<ul><li><a href="bitmap.xml">Bitmap</a></li><li><a href="display.xml">Display</a></li><li><a href="font.xml">Font</a></li><li><a href="iconserver.xml">IconServer</a></li><li><a href="layout.xml">Layout</a></li><li><a href="picture.xml">Picture</a></li><li><a href="pointer.xml">Pointer</a></li><li><a href="surface.xml">Surface</a></li><li><a href="svg.xml">SVG</a></li></ul></li>
                          <li>Network<ul><li><a href="clientsocket.xml">ClientSocket</a></li><li><a href="http.xml">HTTP</a></li><li><a href="netsocket.xml">NetSocket</a></li><li><a href="proxy.xml">Proxy</a></li></ul></li>
                          <li>Vectors<ul><li><a href="vector.xml">Vector</a></li><li><a href="vectorcolour.xml">VectorColour</a></li><li><a href="vectorfilter.xml">VectorFilter</a></li><li><a href="vectorgradient.xml">VectorGradient</a></li><li><a href="vectorimage.xml">VectorImage</a></li><li><a href="vectorpath.xml">VectorPath</a></li><li><a href="vectorpattern.xml">VectorPattern</a></li><li><a href="vectorscene.xml">VectorScene</a></li></ul></li>
                          <li>Widgets<ul><li><a href="button.xml">Button</a></li><li><a href="checkbox.xml">Checkbox</a></li><li><a href="clipboard.xml">Clipboard</a></li><li><a href="combobox.xml">Combobox</a></li><li><a href="document.xml">Document</a></li><li><a href="image.xml">Image</a></li><li><a href="input.xml">Input</a></li><li><a href="menu.xml">Menu</a></li><li><a href="menuitem.xml">MenuItem</a></li><li><a href="resize.xml">Resize</a></li><li><a href="scroll.xml">Scroll</a></li><li><a href="scrollbar.xml">Scrollbar</a></li><li><a href="svgimage.xml">SVGImage</a></li><li><a href="tabfocus.xml">TabFocus</a></li><li><a href="text.xml">Text</a></li><li><a href="view.xml">View</a></li><li><a href="window.xml">Window</a></li></ul></li>
                        </ul>
                      </div>
                    </div>
                  </div>
                </div>
              </div>
            </div>
          </div> <!-- row -->
        </div> <!-- container -->

        <script src="https://ajax.googleapis.com/ajax/libs/jquery/1.12.4/jquery.min.js"></script>
        <script src="../../js/bootstrap.min.js"></script>
        <script src="../../js/base.js"></script>
        <script type="text/javascript">
var glCurrentMethod;

$(document).ready(function() {
   glCurrentMethod = document.getElementById("Introduction");

  $('[data-toggle="tooltip"]').tooltip()

   var page = glParameters["page"];
   if (isEmpty(page)) page = glParameters["function"];

   if (isEmpty(page)) {
      showPage("default-page");
   }
   else showPage(page);
});

function showPage(Name)
{
   var div = document.getElementById(Name);
   if (div) {
      if (glCurrentMethod) { // Hide previous method.
         glCurrentMethod.style.display = "none";
      }
      div.style.display = "block"; // Show selected method.
      glCurrentMethod = div;
   }
   else console.log("Div for '" + Name + "' not found.");
}
         </script>
      </body>
    </html>
  </xsl:template>
</xsl:stylesheet>

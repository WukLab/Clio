name := "SpinalTemplateSbt"

version := "1.0"

scalaVersion := "2.11.12"

libraryDependencies ++= Seq(
  "com.github.spinalhdl" % "spinalhdl-core_2.11" % "1.3.8",
  "com.github.spinalhdl" % "spinalhdl-lib_2.11" % "1.3.8",
  "org.scodec" %% "scodec-bits" % "1.1.12",
  "org.scodec" %% "scodec-core" % "1.11.4"
)

fork := true
